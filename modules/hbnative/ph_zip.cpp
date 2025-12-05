/**************************************************************************/
/*  file_access_zip.cpp                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifdef MINIZIP_ENABLED

#include "ph_zip.h"

#include "core/io/file_access.h"

extern "C" {

struct ZipData {
	Ref<FileAccess> f;
};

static void *phzip_godot_open(voidpf opaque, const char *p_fname, int mode) {
	if (mode & ZLIB_FILEFUNC_MODE_WRITE) {
		return nullptr;
	}

	Ref<FileAccess> f = FileAccess::open(String::utf8(p_fname), FileAccess::READ);
	ERR_FAIL_COND_V(f.is_null(), nullptr);

	ZipData *zd = memnew(ZipData);
	zd->f = f;
	return zd;
}

static uLong phzip_godot_read(voidpf opaque, voidpf stream, void *buf, uLong size) {
	ZipData *zd = (ZipData *)stream;
	zd->f->get_buffer((uint8_t *)buf, size);
	return size;
}

static uLong phzip_godot_write(voidpf opaque, voidpf stream, const void *buf, uLong size) {
	return 0;
}

static long phzip_godot_tell(voidpf opaque, voidpf stream) {
	ZipData *zd = (ZipData *)stream;
	return zd->f->get_position();
}

static long phzip_godot_seek(voidpf opaque, voidpf stream, uLong offset, int origin) {
	ZipData *zd = (ZipData *)stream;

	uint64_t pos = offset;
	switch (origin) {
		case ZLIB_FILEFUNC_SEEK_CUR:
			pos = zd->f->get_position() + offset;
			break;
		case ZLIB_FILEFUNC_SEEK_END:
			pos = zd->f->get_length() + offset;
			break;
		default:
			break;
	}

	zd->f->seek(pos);
	return 0;
}

static int phzip_godot_close(voidpf opaque, voidpf stream) {
	ZipData *zd = (ZipData *)stream;
	memdelete(zd);
	return 0;
}

static int phzip_godot_testerror(voidpf opaque, voidpf stream) {
	ZipData *zd = (ZipData *)stream;
	return zd->f->get_error() != OK ? 1 : 0;
}

static voidpf phzip_godot_alloc(voidpf opaque, uInt items, uInt size) {
	return memalloc((size_t)items * size);
}

static void phzip_godot_free(voidpf opaque, voidpf address) {
	memfree(address);
}
} // extern "C"

void PHZipArchive::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_file", "path"), &PHZipArchive::get_file);
	ClassDB::bind_method(D_METHOD("file_exists", "path"), &PHZipArchive::file_exists);
	ClassDB::bind_method(D_METHOD("try_open_pack", "path", "replace_files", "offset"), &PHZipArchive::try_open_pack);
}

void PHZipArchive::close_handle(unzFile p_file) const {
	ERR_FAIL_NULL_MSG(p_file, "Cannot close a file if none is open.");
	unzCloseCurrentFile(p_file);
	unzClose(p_file);
}

unzFile PHZipArchive::get_file_handle(const String &p_file) const {
	ERR_FAIL_COND_V_MSG(!file_exists(p_file), nullptr, vformat("File '%s' doesn't exist.", p_file));
	File file = files[p_file];

	zlib_filefunc_def io;
	memset(&io, 0, sizeof(io));

	io.opaque = nullptr;
	io.zopen_file = phzip_godot_open;
	io.zread_file = phzip_godot_read;
	io.zwrite_file = phzip_godot_write;

	io.ztell_file = phzip_godot_tell;
	io.zseek_file = phzip_godot_seek;
	io.zclose_file = phzip_godot_close;
	io.zerror_file = phzip_godot_testerror;

	io.alloc_mem = phzip_godot_alloc;
	io.free_mem = phzip_godot_free;

	unzFile pkg = unzOpen2(packages[file.package].filename.utf8().get_data(), &io);
	ERR_FAIL_NULL_V_MSG(pkg, nullptr, vformat("Cannot open file '%s'.", packages[file.package].filename));
	int unz_err = unzGoToFilePos(pkg, &file.file_pos);
	if (unz_err != UNZ_OK || unzOpenCurrentFile(pkg) != UNZ_OK) {
		unzClose(pkg);
		ERR_FAIL_V(nullptr);
	}

	return pkg;
}

bool PHZipArchive::try_open_pack(const String &p_path, bool p_replace_files, uint64_t p_offset = 0) {
	// load with offset feature only supported for PCK files
	ERR_FAIL_COND_V_MSG(p_offset != 0, false, "Invalid PCK data. Note that loading files with a non-zero offset isn't supported with ZIP archives.");

	zlib_filefunc_def io;
	memset(&io, 0, sizeof(io));

	io.opaque = nullptr;
	io.zopen_file = phzip_godot_open;
	io.zread_file = phzip_godot_read;
	io.zwrite_file = phzip_godot_write;

	io.ztell_file = phzip_godot_tell;
	io.zseek_file = phzip_godot_seek;
	io.zclose_file = phzip_godot_close;
	io.zerror_file = phzip_godot_testerror;

	unzFile zfile = unzOpen2(p_path.utf8().get_data(), &io);
	ERR_FAIL_NULL_V(zfile, false);

	unz_global_info64 gi;
	int err = unzGetGlobalInfo64(zfile, &gi);
	ERR_FAIL_COND_V(err != UNZ_OK, false);

	Package pkg;
	pkg.filename = p_path;
	pkg.zfile = zfile;
	packages.push_back(pkg);
	int pkg_num = packages.size() - 1;

	for (uint64_t i = 0; i < gi.number_entry; i++) {
		char filename_inzip[256];

		unz_file_info64 file_info;
		err = unzGetCurrentFileInfo64(zfile, &file_info, filename_inzip, sizeof(filename_inzip), nullptr, 0, nullptr, 0);
		ERR_CONTINUE(err != UNZ_OK);

		File f;
		f.package = pkg_num;
		unzGetFilePos(zfile, &f.file_pos);

		String fname = String::utf8(filename_inzip);
		files[fname] = f;

		if ((i + 1) < gi.number_entry) {
			unzGoToNextFile(zfile);
		}
	}

	return true;
}

bool PHZipArchive::file_exists(const String &p_name) const {
	return files.has(p_name);
}

Ref<FileAccess> PHZipArchive::get_file(const String &p_path) {
	return memnew(FileAccessPHZip(p_path, this));
}

PHZipArchive::PHZipArchive() {
}

PHZipArchive::~PHZipArchive() {
	for (int i = 0; i < packages.size(); i++) {
		unzClose(packages[i].zfile);
	}

	packages.clear();
}

Error FileAccessPHZip::open_internal(const String &p_path, int p_mode_flags) {
	_close();

	ERR_FAIL_COND_V(p_mode_flags & FileAccess::WRITE, FAILED);
	ERR_FAIL_COND_V(!ph_zip.is_valid(), FAILED);
	zfile = ph_zip->get_file_handle(p_path);
	ERR_FAIL_NULL_V(zfile, FAILED);

	int err = unzGetCurrentFileInfo64(zfile, &file_info, nullptr, 0, nullptr, 0, nullptr, 0);
	ERR_FAIL_COND_V(err != UNZ_OK, FAILED);

	return OK;
}

void FileAccessPHZip::_close() {
	if (!zfile) {
		return;
	}

	ERR_FAIL_COND(!ph_zip.is_valid());
	ph_zip->close_handle(zfile);
	zfile = nullptr;
}

bool FileAccessPHZip::is_open() const {
	return zfile != nullptr;
}

void FileAccessPHZip::seek(uint64_t p_position) {
	ERR_FAIL_NULL(zfile);

	unzSeekCurrentFile(zfile, p_position);
}

void FileAccessPHZip::seek_end(int64_t p_position) {
	ERR_FAIL_NULL(zfile);
	unzSeekCurrentFile(zfile, get_length() + p_position);
}

uint64_t FileAccessPHZip::get_position() const {
	ERR_FAIL_NULL_V(zfile, 0);
	return unztell64(zfile);
}

uint64_t FileAccessPHZip::get_length() const {
	ERR_FAIL_NULL_V(zfile, 0);
	return file_info.uncompressed_size;
}

bool FileAccessPHZip::eof_reached() const {
	ERR_FAIL_NULL_V(zfile, true);

	return at_eof;
}

uint64_t FileAccessPHZip::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	ERR_FAIL_COND_V(!p_dst && p_length > 0, -1);
	ERR_FAIL_NULL_V(zfile, -1);

	at_eof = unzeof(zfile);
	if (at_eof) {
		return 0;
	}
	int64_t read = unzReadCurrentFile(zfile, p_dst, p_length);
	ERR_FAIL_COND_V(read < 0, read);
	if ((uint64_t)read < p_length) {
		at_eof = true;
	}
	return read;
}

Error FileAccessPHZip::get_error() const {
	if (!zfile) {
		return ERR_UNCONFIGURED;
	}
	if (eof_reached()) {
		return ERR_FILE_EOF;
	}

	return OK;
}

void FileAccessPHZip::flush() {
	ERR_FAIL();
}

bool FileAccessPHZip::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_V(false);
}

bool FileAccessPHZip::file_exists(const String &p_name) {
	return false;
}

void FileAccessPHZip::close() {
	_close();
}

FileAccessPHZip::FileAccessPHZip(const String &p_path, Ref<PHZipArchive> p_ph_zip) {
	ph_zip = p_ph_zip;
	open_internal(p_path, FileAccess::READ);
}

FileAccessPHZip::~FileAccessPHZip() {
	_close();
}

#endif // MINIZIP_ENABLED
