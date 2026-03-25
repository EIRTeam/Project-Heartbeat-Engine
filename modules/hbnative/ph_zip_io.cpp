#include "ph_zip_io.h"

struct ZipData {
	Ref<FileAccess> f;
};

void *phzipio_open(voidpf opaque, const void *p_fname, int mode) {
	Ref<FileAccess> *fa = reinterpret_cast<Ref<FileAccess> *>(opaque);

	String fname = String::utf8((const char*)p_fname);

	int file_access_mode = 0;
	if (mode & ZLIB_FILEFUNC_MODE_READ) {
		file_access_mode |= FileAccess::READ;
	}
	if (mode & ZLIB_FILEFUNC_MODE_WRITE) {
		file_access_mode |= FileAccess::WRITE;
	}
	if (mode & ZLIB_FILEFUNC_MODE_CREATE) {
		file_access_mode |= FileAccess::WRITE_READ;
	}

	(*fa) = FileAccess::open(fname, file_access_mode);
	if (fa->is_null()) {
		return nullptr;
	}

	return opaque;
}

uLong phzipio_read(voidpf opaque, voidpf stream, void *buf, uLong size) {
	Ref<FileAccess> *fa = reinterpret_cast<Ref<FileAccess> *>(opaque);
	ERR_FAIL_NULL_V(fa, 0);
	ERR_FAIL_COND_V(fa->is_null(), 0);

	return (*fa)->get_buffer((uint8_t *)buf, size);
}

uLong phzipio_write(voidpf opaque, voidpf stream, const void *buf, uLong size) {
	Ref<FileAccess> *fa = reinterpret_cast<Ref<FileAccess> *>(opaque);
	ERR_FAIL_NULL_V(fa, 0);
	ERR_FAIL_COND_V(fa->is_null(), 0);

	(*fa)->store_buffer((uint8_t *)buf, size);
	return size;
}

ZPOS64_T phzipio_tell(voidpf opaque, voidpf stream) {
	Ref<FileAccess> *fa = reinterpret_cast<Ref<FileAccess> *>(opaque);
	ERR_FAIL_NULL_V(fa, 0);
	ERR_FAIL_COND_V(fa->is_null(), 0);

	return (*fa)->get_position();
}

long phzipio_seek(voidpf opaque, voidpf stream, ZPOS64_T offset, int origin) {
	Ref<FileAccess> *fa = reinterpret_cast<Ref<FileAccess> *>(opaque);
	ERR_FAIL_NULL_V(fa, 0);
	ERR_FAIL_COND_V(fa->is_null(), 0);

	uint64_t pos = offset;
	switch (origin) {
		case ZLIB_FILEFUNC_SEEK_CUR:
			pos = (*fa)->get_position() + offset;
			break;
		case ZLIB_FILEFUNC_SEEK_END:
			pos = (*fa)->get_length() + offset;
			break;
		default:
			break;
	}

	(*fa)->seek(pos);
	return 0;
}

int phzipio_close(voidpf opaque, voidpf stream) {
	Ref<FileAccess> *fa = reinterpret_cast<Ref<FileAccess> *>(opaque);
	ERR_FAIL_NULL_V(fa, 0);
	ERR_FAIL_COND_V(fa->is_null(), 0);

	fa->unref();
	return 0;
}

int phzipio_testerror(voidpf opaque, voidpf stream) {
	Ref<FileAccess> *fa = reinterpret_cast<Ref<FileAccess> *>(opaque);
	ERR_FAIL_NULL_V(fa, 1);
	ERR_FAIL_COND_V(fa->is_null(), 0);

	return (fa->is_valid() && (*fa)->get_error() != OK) ? 1 : 0;
}

voidpf phzipio_alloc(voidpf opaque, uInt items, uInt size) {
	voidpf ptr = memalloc_zeroed((size_t)items * size);
	return ptr;
}

void phzipio_free(voidpf opaque, voidpf address) {
	memfree(address);
}

zlib_filefunc64_def phzipio_create_io(Ref<FileAccess> *p_data) {
	zlib_filefunc64_def io;
	io.opaque = (void *)p_data;
	io.zopen64_file = phzipio_open;
	io.zread_file = phzipio_read;
	io.zwrite_file = phzipio_write;
	io.ztell64_file = phzipio_tell;
	io.zseek64_file = phzipio_seek;
	io.zclose_file = phzipio_close;
	io.zerror_file = phzipio_testerror;
	io.alloc_mem = phzipio_alloc;
	io.free_mem = phzipio_free;
	return io;
}