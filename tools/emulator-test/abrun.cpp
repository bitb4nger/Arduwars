// abrun: headless Arduboy runner on the Ardens core.
// usage: abrun game.hex [fxdata.bin] -f FRAMES -o OUTDIR [-e EVERY] [-s snap,snap] [-i "f:BTNS,f:..."] [-I inputfile]
// Buttons: U D L R A B, joined (e.g. "f120:AB" -> "120:AB"). Writes OUTDIR/fNNNNN.pgm (128x64).
// Prints serial output and, at the end, the lowest stack pointer seen.
#include "absim.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
static std::vector<std::pair<int,std::string>> parseInput(const std::string &s){
    std::vector<std::pair<int,std::string>> v; std::stringstream ss(s); std::string t;
    while(std::getline(ss,t,',')){ auto c=t.find(':'); if(c==std::string::npos) continue;
        v.push_back({atoi(t.substr(0,c).c_str()), t.substr(c+1)}); }
    std::stable_sort(v.begin(),v.end(),[](auto&a,auto&b){return a.first<b.first;}); return v;
}
int main(int argc,char**argv){
    std::vector<std::string> files; int frames=600, every=0; std::string out, input; std::vector<int> snaps; bool trace=false; std::vector<std::string> script; FILE* cpuf=nullptr; FILE* proff=nullptr; int profFrom=0; bool mini=false;
    for(int i=1;i<argc;i++){ std::string a=argv[i];
        if(a=="-f") frames=atoi(argv[++i]); else if(a=="-o") out=argv[++i]; else if(a=="-e") every=atoi(argv[++i]);
        else if(a=="-i") input+=std::string(argv[++i])+",";
        else if(a=="-t") trace=true;
        else if(a=="-c") cpuf=fopen(argv[++i],"w");
        else if(a=="-m") mini=true;
        else if(a=="-P"){ proff=fopen(argv[++i],"w"); profFrom=atoi(argv[++i]); }
        else if(a=="-S"){ std::ifstream f(argv[++i]); std::string l; while(std::getline(f,l)) if(!l.empty()&&l[0]!='#') script.push_back(l); }
        else if(a=="-I"){ std::ifstream f(argv[++i]); std::string l; while(std::getline(f,l)) if(!l.empty()&&l[0]!='#') input+=l+","; }
        else if(a=="-s"){ std::stringstream ss(argv[++i]); std::string t; while(std::getline(ss,t,',')) snaps.push_back(atoi(t.c_str())); }
        else files.push_back(a); }
    auto ab=std::make_unique<absim::arduboy_t>();
    if(mini){ ab->program_state.cfg.fxport_reg=absim::reg::addr::PORTE; ab->program_state.cfg.fxport_mask=absim::reg::bit::PORTE::PORTE2; }
    for(auto&fn:files){ std::ifstream f(fn,std::ios::binary); auto e=ab->load_file(fn.c_str(),f); if(!e.empty()){fprintf(stderr,"%s: %s\n",fn.c_str(),e.c_str());return 1;} }
    if(mini){ ab->program_state.cfg.fxport_reg=absim::reg::addr::PORTE; ab->program_state.cfg.fxport_mask=absim::reg::bit::PORTE::PORTE2; ab->reset(); ab->reload_fx(); }
    fprintf(stderr,"device %s fxport 0x%02x mask 0x%02x\n",ab->program_state.device_type.c_str(),(unsigned)ab->peripherals.fxport_reg,(unsigned)ab->peripherals.fxport_mask);
    auto in=parseInput(input); size_t ip=0; std::string cur;
    ab->core_state.cpu.enabled_autobreaks.reset();
    ab->debugger_state.paused=false;
    ab->profiler_reset(); ab->profiler_state.enabled=true;
    ab->peripherals.display.enable_filter=false;
    uint32_t minsp=0xffff;
    std::string serialAll; size_t sp_i=0; int base=0; bool waiting=false; std::string waitTok; size_t waitFrom=0;
    std::vector<std::pair<int,std::string>> pend; std::vector<std::pair<int,std::string>> snapNames;
    for(int fr=1;fr<=frames;fr++){
        while(ip<in.size()&&in[ip].first<=fr){cur=in[ip].second;ip++;}
        // scripted input
        while(!waiting && sp_i<script.size()){
            std::string l=script[sp_i];
            if(l[0]=='W'){ waitTok=l.substr(2); waiting=true; waitFrom=serialAll.size(); sp_i++; break; }
            else if(l[0]=='+'){ int n=atoi(l.c_str()+1); auto sp=l.find(' '); std::string b= sp==std::string::npos? "": l.substr(sp+1);
                pend.push_back({base+n,b}); base+=n; sp_i++; }
            else if(l.rfind("snap",0)==0){ snapNames.push_back({base,l.substr(5)}); sp_i++; }
            else sp_i++;
        }
        for(auto it=pend.begin();it!=pend.end();){ if(it->first<=fr){cur=it->second; it=pend.erase(it);} else ++it; }
        uint8_t pinf=0xF0, pine=0x40, pinb=0x10;
        for(char c:cur){ if(c=='U')pinf&=~0x80; if(c=='R')pinf&=~0x40; if(c=='D')pinf&=~0x10; if(c=='L')pinf&=~0x20; if(c=='A')pine&=~0x40; if(c=='B')pinb&=~0x10; }
        auto &cpu=ab->core_state.cpu;
        cpu.data[absim::reg::addr::PINB]=pinb; cpu.data[absim::reg::addr::PINE]=pine; cpu.data[absim::reg::addr::PINF]=pinf;
        if(trace){
            static uint32_t best=0xffff;
            uint64_t target=cpu.cycle_count+266667;
            while(cpu.cycle_count<target){ ab->cycle(); uint16_t sp=cpu.sp();
                if(sp<best && cpu.pushed_at_least_once){ best=sp; fprintf(stderr,"SP 0x%04x frame %d pc 0x%04x stack:",sp,fr,cpu.pc*2);
                    for(uint32_t k=0;k<cpu.num_stack_frames;k++) fprintf(stderr," %04x",cpu.stack_frames[k].pc*2);
                    fprintf(stderr,"\n"); } }
        } else
        ab->advance(uint64_t(1e12/60));
        if(!cpu.serial_bytes.empty()){ fwrite(cpu.serial_bytes.data(),1,cpu.serial_bytes.size(),stdout); serialAll.append((const char*)cpu.serial_bytes.data(),cpu.serial_bytes.size()); cpu.serial_bytes.clear(); }
        if(waiting && serialAll.find(waitTok,waitFrom)!=std::string::npos){ waiting=false; base=fr; }
        minsp=std::min<uint32_t>(minsp,cpu.min_stack);
        if(proff && fr==profFrom){ ab->profiler_state.counts.fill(0); }
        if(cpuf){ static uint64_t pt=0,pw=0; auto &pr=ab->profiler_state; fprintf(cpuf,"%d %.0f\n",fr,(pr.total_with_sleep-pw)?100.0*(pr.total-pt)/(pr.total_with_sleep-pw):0.0); pt=pr.total; pw=pr.total_with_sleep; }
        bool snap=(every&&fr%every==0); for(int s:snaps) if(s==fr) snap=true;
        for(auto &sn:snapNames) if(sn.first==fr && !out.empty()){ char n[512]; snprintf(n,sizeof n,"%s/%s.pgm",out.c_str(),sn.second.c_str()); FILE*f=fopen(n,"wb"); fprintf(f,"P5 128 64 255\n");
            auto &px=ab->peripherals.display.filtered_pixels; fwrite(px.data(),1,8192,f); fclose(f); }
        if(snap&&!out.empty()){ char n[512]; snprintf(n,sizeof n,"%s/f%05d.pgm",out.c_str(),fr); FILE*f=fopen(n,"wb"); fprintf(f,"P5 128 64 255\n");
            auto &px=ab->peripherals.display.filtered_pixels; fwrite(px.data(),1,8192,f); fclose(f); }
    }
    if(proff){ auto &c=ab->profiler_state.counts; for(size_t k=0;k<c.size();k++) if(c[k]) fprintf(proff,"%u %llu\n",(unsigned)(k*2),(unsigned long long)c[k]); fclose(proff); }
    fflush(stdout);
    { auto &pr=ab->profiler_state; fprintf(stderr,"cpu %.1f%% ",pr.total_with_sleep? 100.0*pr.total/pr.total_with_sleep:0.0); }
    fprintf(stderr,"min SP 0x%04x (RAM ends 0x0AFF)\n",minsp);
    return 0;
}
