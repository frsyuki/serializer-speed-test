#include <string.h>
#include <sys/time.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <limits>

#include <msgpack.hpp>
#include "speedtest_msgpack.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include "speedtest_protobuf.pb.h"
#include "speedtest_protobuf.pb.cc"

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>

class simple_timer {
public:
	void reset() { gettimeofday(&m_timeval, NULL); }
	double show_stat(size_t bufsz)
	{
		struct timeval endtime;
		gettimeofday(&endtime, NULL);
		double sec = (endtime.tv_sec - m_timeval.tv_sec)
			+ (double)(endtime.tv_usec - m_timeval.tv_usec) / 1000 / 1000;
		std::cout << sec << " sec" << std::endl;
		std::cout << (double(bufsz)/1024/1024) << " MB" << std::endl;
		std::cout << (bufsz/sec/1000/1000*8) << " Mbps" << std::endl;
		return sec;
	}
private:
	timeval m_timeval;
};

char* TASK_STR_PTR;
static const unsigned int TASK_STR_LEN = 1<<9;

void test_msgpack(unsigned int num)
{
	type_msgpack::Test target;
	target.id        = 13;
	target.width     = 250;
	target.height    = 110;
	target.data.ptr  = TASK_STR_PTR;
	target.data.size = TASK_STR_LEN;

	msgpack::sbuffer raw;
	{
		msgpack::packer<msgpack::sbuffer> pk(raw);
		for(unsigned int i=0; i < num; ++i) {
			pk << target;
		}
	}

	simple_timer timer;

	std::cout << "---- MessagePack serialize" << std::endl;
	timer.reset();
	{
		msgpack::vrefbuffer buf;
		msgpack::packer<msgpack::vrefbuffer> pk(buf);
		for(unsigned int i=0; i < num; ++i) {
			pk << target;
		}
	}
	timer.show_stat(raw.size());

	std::cout << "---- MessagePack deserialize" << std::endl;
	timer.reset();
	{
		msgpack::zone z;
		size_t off = 0;
		type_msgpack::Test msg;
		for(unsigned int i=0; i < num; ++i) {
			msgpack::object obj = msgpack::unpack(raw.data(), raw.size(), z, &off);
			//msg = obj.convert();
			obj.convert(&msg);
		}
	}
	timer.show_stat(raw.size());
}

void test_protobuf(unsigned int num)
{
	type_protobuf::Test target;
	target.set_id(13);
	target.set_width(250);
	target.set_height(110);
	target.set_data( std::string(TASK_STR_PTR, TASK_STR_LEN) );

	simple_timer timer;

	std::cout << "-- Protocol Buffers serialize" << std::endl;
	timer.reset();
	std::string raw;
	{
		google::protobuf::io::StringOutputStream output(&raw);
		google::protobuf::io::CodedOutputStream encoder(&output);
		for(unsigned int i=0; i < num; ++i) {
			encoder.WriteVarint32(target.ByteSize());
			target.SerializeToCodedStream(&encoder);
		}
	}
	timer.show_stat(raw.size());

	std::cout << "-- Protocol Buffers deserialize" << std::endl;
	timer.reset();
	{
		type_protobuf::Test msg;
		google::protobuf::io::ArrayInputStream input(raw.data(), raw.size());
		google::protobuf::io::CodedInputStream decoder(&input);
		decoder.SetTotalBytesLimit(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
		for(unsigned int i=0; i < num; ++i) {
			uint32_t limit = 0;
			decoder.ReadVarint32(&limit);
			int old = decoder.PushLimit(limit);
			msg.ParseFromCodedStream(&decoder);
			decoder.PopLimit(old);
		}
	}
	timer.show_stat(raw.size());
}


static int yajl_reformat_null(void * ctx) { return 1; }
static int yajl_reformat_boolean(void * ctx, int boolean) { return 1; }
static int yajl_reformat_number(void * ctx, const char * s, unsigned int l) { return 1; }
static int yajl_reformat_string(void * ctx, const unsigned char * stringVal, unsigned int stringLen) { return 1; }
static int yajl_reformat_map_key(void * ctx, const unsigned char * stringVal, unsigned int stringLen) { return 1; }
static int yajl_reformat_start_map(void * ctx) { return 1; }
static int yajl_reformat_end_map(void * ctx) { return 1; }
static int yajl_reformat_start_array(void * ctx) { return 1; }
static int yajl_reformat_end_array(void * ctx) { return 1; }

void test_yajl(unsigned int num)
{
	int32_t id       = 13;
	uint16_t width   = 250;
	uint16_t height  = 110;
	const unsigned char* data_ptr = (const unsigned char*)TASK_STR_PTR;
	size_t data_size = TASK_STR_LEN;

	simple_timer timer;

	std::cout << "-- YAJL serialize" << std::endl;
	timer.reset();
	const unsigned char* raw;
	unsigned int raw_len;
	yajl_gen_config gcfg = {0, NULL};
	yajl_gen g = yajl_gen_alloc(&gcfg, NULL);
	{
		yajl_gen_array_open(g);
		for(unsigned int i=0; i < num; ++i) {
			yajl_gen_array_open(g);
			yajl_gen_integer(g, id);
			yajl_gen_integer(g, width);
			yajl_gen_integer(g, height);
			yajl_gen_string(g, data_ptr, data_size);
			yajl_gen_array_close(g);
		}
		yajl_gen_array_close(g);
		yajl_gen_get_buf(g, &raw, &raw_len);
	}
	timer.show_stat(raw_len);

	std::cout << "-- YAJL deserialize" << std::endl;
	timer.reset();
	{
		yajl_parser_config hcfg = {0, 0};
		yajl_callbacks callbacks = {
			yajl_reformat_null,
			yajl_reformat_boolean,
			NULL,
			NULL,
			yajl_reformat_number,
			yajl_reformat_string,
			yajl_reformat_start_map,
			yajl_reformat_map_key,
			yajl_reformat_end_map,
			yajl_reformat_start_array,
			yajl_reformat_end_array
		};
		yajl_handle h = yajl_alloc(&callbacks, &hcfg, NULL, NULL);

		yajl_status stat = yajl_parse(h, raw, raw_len);
		if (stat != yajl_status_ok && stat != yajl_status_insufficient_data) {
			unsigned char * err = yajl_get_error(h, 1, raw, raw_len);
			std::cerr << err << std::endl;
		}

		yajl_free(h);
	}
	timer.show_stat(raw_len);

	yajl_gen_free(g);
}

int main(int argc, char* argv[])
{
	TASK_STR_PTR = (char*)malloc(TASK_STR_LEN+1);
	memset(TASK_STR_PTR, 'a', TASK_STR_LEN);
	TASK_STR_PTR[TASK_STR_LEN] = '\0';

	if(argc != 2) {
		std::cout << "usage: "<<argv[0]<<" <num>" << std::endl;
		exit(1);
	}

	unsigned int num = atoi(argv[1]);

	test_msgpack(num);  std::cout << "\n";
	test_protobuf(num); std::cout << "\n";
	test_yajl(num);     std::cout << "\n";
}

