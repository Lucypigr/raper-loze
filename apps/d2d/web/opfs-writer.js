// SPDX-License-Identifier: GPL-3.0-or-later
// D2Decomp — copies files the player picked into the origin-private file
// system (OPFS), where the browser build reads them (WasmFS mounts it at
// /opfs). A worker, for the synchronous access handles Safari has had
// since 15.2, and in 8 MB slices, so an MPQ is never whole in memory.
//
// In:  { path: "d2/d2data.mpq", file: File }   (path under the OPFS root)
// Out: { path, written, size } as it goes, then { path, done: true },
//      or { path, error }.
"use strict";

const kSlice = 8 << 20;

async function directoryOf(path) {
  let directory = await navigator.storage.getDirectory();
  const parts = path.split("/");
  for (const part of parts.slice(0, -1)) directory = await directory.getDirectoryHandle(part, { create: true });
  return [directory, parts[parts.length - 1]];
}

self.onmessage = async ({ data: { path, file } }) => {
  let access = null;
  try {
    const [directory, name] = await directoryOf(path);
    const handle = await directory.getFileHandle(name, { create: true });
    access = await handle.createSyncAccessHandle();
    access.truncate(0);
    for (let offset = 0; offset < file.size; offset += kSlice) {
      const slice = new Uint8Array(await file.slice(offset, offset + kSlice).arrayBuffer());
      access.write(slice, { at: offset });
      self.postMessage({ path, written: offset + slice.length, size: file.size });
    }
    access.flush();
    self.postMessage({ path, done: true });
  } catch (error) {
    self.postMessage({ path, error: String(error && error.message || error) });
  } finally {
    if (access) access.close();
  }
};
