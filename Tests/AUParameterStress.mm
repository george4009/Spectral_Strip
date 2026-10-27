// Run against a built AU binary without installing it. Exercises asynchronous list
// invalidation while an AU host repeatedly asks for parameter sizes and contents.
#import <Cocoa/Cocoa.h>
#import <AudioToolbox/AudioToolbox.h>
#include <dlfcn.h>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include <stdexcept>
static void check(OSStatus status) { if(status) throw std::runtime_error("AU status " + std::to_string(status)); }
int main(int argc, char** argv)
{
    @autoreleasepool {
        if(argc!=2) return 2;
        [NSApplication sharedApplication];
        void* lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
        if(!lib) { std::cerr<<dlerror()<<'\n'; return 2; }
        auto factory=reinterpret_cast<AudioComponentFactoryFunction>(dlsym(lib,"Spectral_StripAUFactory"));
        if(!factory) return 2;
        AudioComponentDescription desc {kAudioUnitType_Effect,'SpSt','GiAu',0,0};
        AudioComponent component=AudioComponentRegister(&desc,CFSTR("Spectral Strip stress"),0x0007000f,factory);
        AudioUnit unit=nullptr;check(AudioComponentInstanceNew(component,&unit));
        CFPropertyListRef base=nullptr;UInt32 bytes=sizeof(base);
        check(AudioUnitGetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&base,&bytes));
        NSDictionary* dict=(NSDictionary*)base;
        NSString* stateKey=nil;
        NSData* original=nil;
        for(NSString* key in dict) {
            id value=[dict objectForKey:key];
            if([value isKindOfClass:[NSData class]] && [value length]>8) {
                uint32_t magic=0;memcpy(&magic,[value bytes],4);
                if(magic==0x21324356) { stateKey=key;original=value;break; }
            }
        }
        if(!original) { std::cerr<<"Missing JUCE state\n";return 2; }
        const char* raw=static_cast<const char*>([original bytes]);
        uint32_t length=0;memcpy(&length,raw+4,4);
        std::string xml(raw+8,length);
        const std::string attribute="moduleChain=\"";
        auto start=xml.find(attribute);
        if(start==std::string::npos) return 2;
        start+=attribute.size();auto end=xml.find('"',start);
        std::vector<CFPropertyListRef> states;
        for(int count: {0,3,20,1,12}) {
            std::string chain;
            for(int slot=0;slot<count;++slot) {
                if(slot) chain+=',';
                int type=1+slot%6;
                chain+=std::to_string(slot<15 ? (type<<4)|(slot+1) : (type<<8)|(slot+1));
            }
            auto changed=xml;changed.replace(start,end-start,chain);
            uint32_t size=static_cast<uint32_t>(changed.size()),magic=0x21324356;
            std::vector<char> binary(size+9,0);memcpy(binary.data(),&magic,4);memcpy(binary.data()+4,&size,4);memcpy(binary.data()+8,changed.data(),size);
            auto copy=CFDictionaryCreateMutableCopy(nullptr,0,(CFDictionaryRef)base);
            auto data=CFDataCreate(nullptr,(const UInt8*)binary.data(),binary.size());
            CFDictionarySetValue(copy,(CFStringRef)stateKey,data);CFRelease(data);states.push_back(copy);
        }
        std::atomic<bool> done {false},failed {false};std::atomic<unsigned> queries {0};
        const auto readLists=[&] {
            while(!done.load()) {
                UInt32 size=0;Boolean writable=false;
                if(AudioUnitGetPropertyInfo(unit,kAudioUnitProperty_ParameterList,kAudioUnitScope_Global,0,&size,&writable)!=noErr) { failed=true;break; }
                // Padding detects a write beyond the declared client buffer even without ASan.
                std::vector<uint32_t> ids(size/4+16,0xdeadbeef);
                UInt32 supplied=size;
                auto status=AudioUnitGetProperty(unit,kAudioUnitProperty_ParameterList,kAudioUnitScope_Global,0,ids.data(),&supplied);
                if(status!=noErr || supplied>size) { failed=true;break; }
                for(size_t i=size/4;i<ids.size();++i) if(ids[i]!=0xdeadbeef) failed=true;
                ++queries;
            }
        };
        std::thread reader(readLists);
        std::thread reader2(readLists);
        std::thread writer([&] {
            for(int i=0;i<2000 && !failed.load();++i) {
                auto state=states[static_cast<size_t>(i)%states.size()];
                if(AudioUnitSetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&state,sizeof(state))!=noErr) failed=true;
                const unsigned target=queries.load()+10;
                while(queries.load()<target && !failed.load()) std::this_thread::yield();
            }
            done=true;
        });
        while(!done.load()) [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.001]];
        writer.join();reader.join();reader2.join();
        // Drain the last asynchronous invalidation, then check the final filtered list.
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.05]];
        UInt32 size=0;Boolean writable=false;
        check(AudioUnitGetPropertyInfo(unit,kAudioUnitProperty_ParameterList,kAudioUnitScope_Global,0,&size,&writable));
        if(size/4!=68) failed=true;
        std::cout<<(failed.load()?"FAIL":"PASS")<<" AU concurrent state/list queries: "<<queries.load()<<" reads, 2000 chain changes, final parameters "<<size/4<<'\n';
        for(auto state:states) CFRelease(state);
        CFRelease(base);check(AudioComponentInstanceDispose(unit));
        return failed.load() || queries.load()==0 ? 1 : 0;
    }
}
