#include "registers.hpp"
#include <cmath>
#include <limits>
namespace manipulator::modbus {
static_assert(std::numeric_limits<float>::is_iec559);
namespace {
bool range(uint16_t address,uint16_t count,uint16_t base,uint16_t size) {
    return count && address>=base && uint32_t(address)+count<=uint32_t(base)+size;
}
bool complete(const bool* written,std::size_t count) {
    for(std::size_t i=0;i<count;++i) if(!written[i]) return false;
    return true;
}
}
Exception Registers::translate(ServiceStatus s) {
    switch(s) {
    case ServiceStatus::ok: case ServiceStatus::pending: return Exception::none;
    case ServiceStatus::full: case ServiceStatus::not_ready: return Exception::busy;
    default: return Exception::value;
    }
}
Exception Registers::read_holding(uint16_t a,uint16_t n,uint16_t* out) const {
    if(!out || !n || n>125) return Exception::value;
    const uint16_t* source=nullptr;
    if(range(a,n,model_address,model_words)) source=model_.data()+a-model_address;
    else if(range(a,n,request_address,request_words)) source=request_.data()+a-request_address;
    else return Exception::address;
    for(unsigned i=0;i<n;++i) out[i]=source[i];
    return Exception::none;
}
Exception Registers::write_holding(uint16_t a,uint16_t n,const uint16_t* in) {
    if(!in || !n || n>123) return Exception::value;
    if(a==command_address) return n==1?execute(*in):Exception::address;
    uint16_t* target; bool* written;
    if(range(a,n,model_address,model_words)) {
        const auto s=service_.snapshot();
        if(s.runtime || s.configuring) return Exception::busy;
        target=model_.data()+a-model_address; written=model_written_.data()+a-model_address;
    } else if(range(a,n,request_address,request_words)) {
        target=request_.data()+a-request_address; written=request_written_.data()+a-request_address;
    } else return Exception::address;
    // Validation of range/state precedes every mutation; FC16 is atomic within this bank.
    for(unsigned i=0;i<n;++i) {target[i]=in[i]; written[i]=true;}
    return Exception::none;
}
Exception Registers::execute(uint16_t command) {
    switch(static_cast<Command>(command)) {
    case Command::begin: {
        auto e=translate(service_.begin_configuration());
        if(e==Exception::none) {
            model_.fill(0); request_.fill(0);
            model_written_.fill(false); request_written_.fill(false);
        }
        return e;
    }
    case Command::apply: {
        if(!model_written_[0]) return Exception::value;
        Configuration c; c.count=model_[0];
        if(c.count<3 || c.count>max_joints) return Exception::value;
        // 16 words/joint: a, alpha, d, theta, min, max, scale reserved, step reserved.
        // Reserved scale/step are not used: parameters belong to firmware.
        for(std::size_t i=0;i<c.count;++i) {
            const auto offset=1+i*16;
            if(!complete(model_written_.data()+offset,12)) return Exception::value;
            const auto* p=model_.data()+offset;
            for(unsigned j=0;j<12;j+=2) if(!std::isfinite(decode_float(p+j))) return Exception::value;
            c.geometry[i]={K_REVOLUTE,decode_float(p),decode_float(p+2),decode_float(p+4),decode_float(p+6)};
            c.limits[i]={K_BOUNDED,decode_float(p+8),decode_float(p+10)};
            if(c.limits[i].min>c.limits[i].max) return Exception::value;
        }
        if(!complete(model_written_.data()+97,64)) return Exception::value;
        for(unsigned i=0;i<16;++i) {
            c.base.m[i/4][i%4]=decode_float(model_.data()+97+2*i);
            c.tool.m[i/4][i%4]=decode_float(model_.data()+129+2*i);
        }
        if(k_transform_validate(&c.base)!=K_OK || k_transform_validate(&c.tool)!=K_OK) return Exception::value;
        return translate(service_.activate(c));
    }
    case Command::submit: {
        if(!complete(request_written_.data(),request_words)) return Exception::value;
        Request r; r.id=decode_u32(request_.data());
        r.target={decode_float(request_.data()+2),decode_float(request_.data()+4),decode_float(request_.data()+6)};
        if(!std::isfinite(r.target.x)||!std::isfinite(r.target.y)||!std::isfinite(r.target.z)) return Exception::value;
        for(unsigned i=0;i<max_joints;++i) {
            r.initial[i]=decode_float(request_.data()+8+2*i);
            if(!std::isfinite(r.initial[i])) return Exception::value;
        }
        auto e=translate(service_.submit(r));
        if(e==Exception::none) request_written_.fill(false);
        return e;
    }
    case Command::acknowledge:
        if(!complete(request_written_.data(),2)) return Exception::value;
        return translate(service_.acknowledge(decode_u32(request_.data())));
    default: return Exception::value;
    }
}
Exception Registers::read_input(uint16_t a,uint16_t n,uint16_t* out) {
    if(!out || !n || n>125) return Exception::value;
    if(!range(a,n,0,input_words)) return Exception::address;
    const auto s=service_.snapshot();
    std::array<uint16_t,input_words> words{};
    words[0]=1; // protocol version
    words[1]=s.runtime?2:(s.configuring?1:0);
    words[2]=s.busy; words[3]=s.queued; words[4]=s.results;
    words[5]=static_cast<uint16_t>(s.configuration_status); words[6]=s.has_result;
    if(s.has_result) {
        encode_u32(s.result.id,words.data()+7);
        words[9]=static_cast<uint16_t>(s.result.status);
        for(unsigned i=0;i<max_joints;++i) encode_float(s.result.angles[i],words.data()+10+2*i);
        encode_float(s.result.report.residual_valid?s.result.report.residual:0,words.data()+22);
    }
    for(unsigned i=0;i<n;++i) out[i]=words[a+i];
    return Exception::none;
}
}
