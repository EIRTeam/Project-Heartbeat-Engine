/**************************************************************************/
/*  zip_packer.cpp                                                        */
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

#include "ph_zip_packer.h"

#include "core/io/zip_io.h"
#include "core/os/os.h"
#include "hbnative/ph_zip_io.h"

Error PHZIPPacker::open(const String &p_path, ZipAppend p_append) {
	if (fa.is_valid()) {
		close();
	}

	zlib_filefunc64_def io = phzipio_create_io(&fa);
	zf = zipOpen2_64(p_path.utf8().get_data(), p_append, nullptr, &io);
	return zf != nullptr ? OK : FAILED;
}

Error PHZIPPacker::close() {
	ERR_FAIL_COND_V_MSG(fa.is_null(), FAILED, "PHZIPPacker cannot be closed because it is not open.");

	Error err = zipClose(zf, nullptr) == ZIP_OK ? OK : FAILED;
	if (err == OK) {
		DEV_ASSERT(fa.is_null());
		zf = nullptr;
	}

	return err;
}

void PHZIPPacker::set_compression_level(int p_compression_level) {
	ERR_FAIL_COND_MSG(p_compression_level < Z_DEFAULT_COMPRESSION || p_compression_level > Z_BEST_COMPRESSION, "Invalid compression level.");
	compression_level = p_compression_level;
}

int PHZIPPacker::get_compression_level() const {
	return compression_level;
}

Error PHZIPPacker::start_file(const String &p_path) {
	ERR_FAIL_COND_V_MSG(fa.is_null(), FAILED, "PHZIPPacker must be opened before use.");

	zip_fileinfo zipfi;

	OS::DateTime time = OS::get_singleton()->get_datetime();

	zipfi.tmz_date.tm_sec = time.second;
	zipfi.tmz_date.tm_min = time.minute;
	zipfi.tmz_date.tm_hour = time.hour;
	zipfi.tmz_date.tm_mday = time.day;
	zipfi.tmz_date.tm_mon = time.month - 1;
	zipfi.tmz_date.tm_year = time.year;
	zipfi.dosDate = 0;
	zipfi.internal_fa = 0;
	zipfi.external_fa = 0;

	int err = zipOpenNewFileInZip4(zf,
			p_path.utf8().get_data(),
			&zipfi,
			nullptr,
			0,
			nullptr,
			0,
			nullptr,
			Z_DEFLATED,
			compression_level,
			0,
			-MAX_WBITS,
			DEF_MEM_LEVEL,
			Z_DEFAULT_STRATEGY,
			nullptr,
			0,
			0, // "version made by", indicates the compatibility of the file attribute information (the `external_fa` field above).
			1 << 11); // Bit 11 is the language encoding flag. When set, filename and comment fields must be encoded using UTF-8.
	return err == ZIP_OK ? OK : FAILED;
}

Error PHZIPPacker::write_file(const Vector<uint8_t> &p_data) {
	ERR_FAIL_COND_V_MSG(fa.is_null(), FAILED, "PHZIPPacker must be opened before use.");

	return zipWriteInFileInZip(zf, p_data.ptr(), p_data.size()) == ZIP_OK ? OK : FAILED;
}

Error PHZIPPacker::close_file() {
	ERR_FAIL_COND_V_MSG(fa.is_null(), FAILED, "PHZIPPacker must be opened before use.");

	return zipCloseFileInZip(zf) == ZIP_OK ? OK : FAILED;
}

void PHZIPPacker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open", "path", "append"), &PHZIPPacker::open, DEFVAL(Variant(APPEND_CREATE)));
	ClassDB::bind_method(D_METHOD("set_compression_level", "compression_level"), &PHZIPPacker::set_compression_level);
	ClassDB::bind_method(D_METHOD("get_compression_level"), &PHZIPPacker::get_compression_level);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "compression_level"), "set_compression_level", "get_compression_level");
	ClassDB::bind_method(D_METHOD("start_file", "path"), &PHZIPPacker::start_file);
	ClassDB::bind_method(D_METHOD("write_file", "data"), &PHZIPPacker::write_file);
	ClassDB::bind_method(D_METHOD("close_file"), &PHZIPPacker::close_file);
	ClassDB::bind_method(D_METHOD("close"), &PHZIPPacker::close);

	BIND_ENUM_CONSTANT(APPEND_CREATE);
	BIND_ENUM_CONSTANT(APPEND_CREATEAFTER);
	BIND_ENUM_CONSTANT(APPEND_ADDINZIP);

	BIND_ENUM_CONSTANT(COMPRESSION_DEFAULT);
	BIND_ENUM_CONSTANT(COMPRESSION_NONE);
	BIND_ENUM_CONSTANT(COMPRESSION_FAST);
	BIND_ENUM_CONSTANT(COMPRESSION_BEST);
}

PHZIPPacker::PHZIPPacker() {
}

PHZIPPacker::~PHZIPPacker() {
	if (fa.is_valid()) {
		close();
	}
}
