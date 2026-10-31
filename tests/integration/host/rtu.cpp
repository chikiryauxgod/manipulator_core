#include "protocol/modbus/server.hpp"
#include "application/engine.hpp"
#include <array>
#include <cstring>
#include <cstdlib>
#define CHECK(x) do { if(!(x)) std::abort(); } while(false)
using namespace manipulator;
using namespace manipulator::modbus;
struct Wire {
    std::array<uint8_t,256> input{},output{};
    uint16_t size=0,position=0,written=0;
    static int32_t read(uint8_t* p,uint16_t n,int32_t,void* arg) {
        auto& w=*static_cast<Wire*>(arg);
        if(n>w.size-w.position) n=w.size-w.position;
        std::memcpy(p,w.input.data()+w.position,n); w.position+=n; return n;
    }
    static int32_t write(const uint8_t* p,uint16_t n,int32_t,void* arg) {
        auto& w=*static_cast<Wire*>(arg);
        CHECK(w.written+n<=w.output.size());
        std::memcpy(w.output.data()+w.written,p,n); w.written+=n; return n;
    }
    static uint16_t crc(const uint8_t* bytes,unsigned n) {
        uint16_t crc=0xffff;
        for(unsigned i=0;i<n;++i) {
            crc^=bytes[i];
            for(unsigned j=0;j<8;++j) crc=(crc>>1)^((crc&1)?0xa001:0);
        }
        return crc;
    }
    void frame(std::initializer_list<uint8_t> data) {
        position=written=0; size=0;
        for(auto b:data) input[size++]=b;
        auto c=crc(input.data(),size); input[size++]=c&255; input[size++]=c>>8;
    }
    void valid_response() {
        CHECK(written>=5);
        CHECK(crc(output.data(),written)==0);
    }
};
int main() {
    Engine engine; Registers registers(engine); Server server(registers); Wire wire;
    nmbs_platform_conf platform; nmbs_platform_conf_create(&platform);
    platform.transport=NMBS_TRANSPORT_RTU; platform.read=Wire::read;
    platform.write=Wire::write; platform.arg=&wire;
    CHECK(server.initialize(1,platform)==NMBS_ERROR_NONE); server.timeouts(1,1);
    wire.frame({1,6,0,0,0,1}); // BEGIN
    CHECK(server.poll()==NMBS_ERROR_NONE); wire.valid_response();
    CHECK(wire.written==8 && wire.output[1]==6);
    wire.frame({1,4,0,0,0,7}); // input snapshot
    CHECK(server.poll()==NMBS_ERROR_NONE); wire.valid_response();
    CHECK(wire.output[1]==4 && wire.output[2]==14 && wire.output[4]==1);
    wire.frame({1,6,0,0,0,2}); // incomplete APPLY -> exception 03
    CHECK(server.poll()==NMBS_ERROR_NONE); wire.valid_response();
    CHECK(wire.output[1]==0x86 && wire.output[2]==3);
    wire.frame({1,3,0xff,0xff,0,1}); // invalid address
    CHECK(server.poll()==NMBS_ERROR_NONE); wire.valid_response();
    CHECK(wire.output[1]==0x83 && wire.output[2]==2);
    wire.frame({1,6,0,0,0,1}); wire.input[7]^=1; // corrupt CRC
    CHECK(server.poll()==NMBS_ERROR_CRC && wire.written==0);
}
