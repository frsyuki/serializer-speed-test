#include <msgpack.hpp>

namespace type_msgpack {


struct Test {
	int32_t id;
	uint16_t width;
	uint16_t height;
	msgpack::type::raw_ref data;
	MSGPACK_DEFINE(id, width, height, data);
};


}

