#include "../Source/PluginProcessor.h"
#include <stdexcept>
#include <iostream>
#include <thread>
#include <atomic>
class TestTransport : public juce::AudioPlayHead
{
public:
    PositionInfo info;
    juce::Optional<PositionInfo> getPosition() const override { return info; }
};
void runProcessorTests()
{
    juce::ScopedJuceInitialiser_GUI initialise;
    auto processor = std::make_unique<SpectralStripProcessor>();
    processor->setChain({0x11,0x33});
    juce::MemoryBlock chainState; processor->getStateInformation(chainState);
    auto chainRecall=std::make_unique<SpectralStripProcessor>();
    chainRecall->setStateInformation(chainState.getData(),static_cast<int>(chainState.getSize()));
    if(chainRecall->getChain()!=SpectralStripProcessor::Chain{0x11,0x33}) throw std::runtime_error("Module chain recall failed");
    const auto automatable=[&](SpectralStripProcessor& plugin,const juce::String& id)
    { auto* parameter=plugin.state.getParameter(id); if(parameter==nullptr) throw std::runtime_error("Missing automation parameter"); return parameter->isAutomatable(); };
    if(!automatable(*processor,"smear") || automatable(*processor,"threshold") || !automatable(*processor,"delayMs") || !automatable(*processor,"low") || !automatable(*processor,"fftSize")
       || automatable(*processor,SpectralStripProcessor::moduleParamID(0,"blurAmount")) || automatable(*processor,SpectralStripProcessor::moduleParamID(0,"gate")))
        throw std::runtime_error("Automation visibility does not follow the modules in the strip");
    if(!automatable(*chainRecall,"smear") || automatable(*chainRecall,"threshold"))
        throw std::runtime_error("Recalled chain did not update automation visibility");
    if(automatable(*processor,SpectralStripProcessor::moduleParamID(3,"blurAmount")))
        throw std::runtime_error("An unused module is exposed to automation");
    if(!processor->addInstance(4)) throw std::runtime_error("Adding a module failed");
    const int added=SpectralEngine::entrySlot(processor->getChain()[2]);
    if(!automatable(*processor,SpectralStripProcessor::moduleParamID(added,"blurAmount")))
        throw std::runtime_error("Adding a module did not expose its parameters");
    processor->setChain({});
    if(automatable(*processor,"smear") || !automatable(*processor,"mix"))
        throw std::runtime_error("Removing every module left its parameters automatable");
    processor->setChain({0x11,0x33});
    auto legacyTree=processor->state.copyState();
    legacyTree.setProperty("instanceChain", static_cast<juce::int64>(0x332211), nullptr);
    juce::MemoryBlock legacyData; auto legacyXml=legacyTree.createXml(); juce::AudioProcessor::copyXmlToBinary(*legacyXml, legacyData);
    chainRecall->setStateInformation(legacyData.getData(), static_cast<int>(legacyData.getSize()));
    if(chainRecall->getChain()!=SpectralStripProcessor::Chain{0x11,0x22,0x33}) throw std::runtime_error("Legacy eight-module chain was not restored");
    processor->setChain({0x11,0x22,0x33});
    chainRecall->setChain({0x11,0x31,0x99,0x22});
    if(chainRecall->getChain()!=SpectralStripProcessor::Chain{0x11,0x22}) throw std::runtime_error("Invalid chain was not sanitized");
    auto duplicates=std::make_unique<SpectralStripProcessor>(); duplicates->setChain({});
    for(int i=0;i<SpectralEngine::maxModules;++i) if(!duplicates->addInstance(3)) throw std::runtime_error("Duplicate Delay insertion failed");
    if(duplicates->addInstance(3)) throw std::runtime_error("Capacity limit failed");
    const int last=duplicates->getChain()[SpectralEngine::maxModules-1];
    if(SpectralEngine::entryType(last)!=3 || SpectralEngine::entrySlot(last)!=SpectralEngine::maxModules-1)
        throw std::runtime_error("The twentieth module has no independent slot");
    auto* firstTime=duplicates->state.getParameter(SpectralStripProcessor::moduleParamID(0,"delayMs"));
    auto* secondTime=duplicates->state.getParameter(SpectralStripProcessor::moduleParamID(1,"delayMs"));
    firstTime->setValueNotifyingHost(firstTime->convertTo0to1(120)); secondTime->setValueNotifyingHost(secondTime->convertTo0to1(430));
    auto reversed=duplicates->getChain(); std::reverse(reversed.begin(),reversed.end()); duplicates->setChain(reversed);
    juce::MemoryBlock duplicatesData; duplicates->getStateInformation(duplicatesData);
    chainRecall->setStateInformation(duplicatesData.getData(),static_cast<int>(duplicatesData.getSize()));
    if(chainRecall->getChain()!=reversed || std::abs(chainRecall->state.getRawParameterValue(SpectralStripProcessor::moduleParamID(0,"delayMs"))->load()-120)>.01f
       || std::abs(chainRecall->state.getRawParameterValue(SpectralStripProcessor::moduleParamID(1,"delayMs"))->load()-430)>.01f) throw std::runtime_error("Independent duplicate state recall failed");
    auto oldModular=processor->state.copyState(); oldModular.setProperty("chainCode",0x1234,nullptr);
    juce::MemoryBlock oldModularData; auto oldModularXml=oldModular.createXml(); juce::AudioProcessor::copyXmlToBinary(*oldModularXml,oldModularData);
    chainRecall->setStateInformation(oldModularData.getData(),static_cast<int>(oldModularData.getSize()));
    if(chainRecall->getChain()!=SpectralStripProcessor::Chain{0x33,0x22,0x11}) throw std::runtime_error("0.3 migration failed");
    duplicates.reset();
    auto blurState=std::make_unique<SpectralStripProcessor>(); blurState->setChain({});
    if(!blurState->addInstance(4) || !blurState->addInstance(4)) throw std::runtime_error("Duplicate Blur insertion failed");
    const auto setBlur=[&](int slot,const char* id,float value)
    { auto* parameter=blurState->state.getParameter(SpectralStripProcessor::moduleParamID(slot,id)); parameter->setValueNotifyingHost(parameter->convertTo0to1(value)); };
    setBlur(0,"blurWidth",120); setBlur(1,"blurWidth",1600); setBlur(1,"blurAmount",.8f); setBlur(1,"blurMix",.4f);
    blurState->setChain({0x42,0x41}); juce::MemoryBlock blurData; blurState->getStateInformation(blurData);
    chainRecall->setStateInformation(blurData.getData(),static_cast<int>(blurData.getSize()));
    if(chainRecall->getChain()!=SpectralStripProcessor::Chain{0x42,0x41}
       || std::abs(chainRecall->state.getRawParameterValue(SpectralStripProcessor::moduleParamID(0,"blurWidth"))->load()-120)>.01f
       || std::abs(chainRecall->state.getRawParameterValue(SpectralStripProcessor::moduleParamID(1,"blurWidth"))->load()-1600)>.01f
       || std::abs(chainRecall->state.getRawParameterValue(SpectralStripProcessor::moduleParamID(1,"blurMix"))->load()-.4f)>.01f) throw std::runtime_error("Blur state recall failed");
    chainRecall->prepareToPlay(48000,257);
    juce::AudioBuffer<float> blurAudio(2,257); juce::MidiBuffer blurMidi; double blurDifference=0,referenceEnergy=0; int blurTime=0;
    for(int block=0;block<190;++block)
    {
        const int start=blurTime;
        for(int i=0;i<257;++i,++blurTime)
            for(int channel=0;channel<2;++channel) blurAudio.setSample(channel,i,.2f*std::sin(juce::MathConstants<float>::twoPi*1500*static_cast<float>(blurTime)/48000));
        chainRecall->processBlock(blurAudio,blurMidi);
        if(block>150) for(int i=0;i<257;++i)
        {
            const float expected=.2f*std::sin(juce::MathConstants<float>::twoPi*1500*static_cast<float>(start+i-2048)/48000);
            const float sample=blurAudio.getSample(0,i); if(!std::isfinite(sample)) throw std::runtime_error("Processor Blur is not finite");
            blurDifference+=(sample-expected)*(sample-expected); referenceEnergy+=expected*expected;
        }
    }
    if(blurDifference<referenceEnergy*.01) throw std::runtime_error("Blur parameters did not reach processor audio");
    std::cout << "PASS processor Blur audio and independent state recall\n";
    blurState.reset();
    auto trem=std::make_unique<SpectralStripProcessor>(); trem->setChain({});
    if(!trem->addInstance(5) || !trem->addInstance(5)) throw std::runtime_error("Duplicate Tremolo insertion failed");
    const auto setTrem=[&](int slot,const char* id,float value)
    { auto* p=trem->state.getParameter(SpectralStripProcessor::moduleParamID(slot,id)); p->setValueNotifyingHost(p->convertTo0to1(value)); };
    setTrem(0,"tremoloRate",2); setTrem(1,"tremoloRate",7); setTrem(1,"tremoloShape",.8f); setTrem(0,"tremoloDepth",1);
    trem->setChain({0x52,0x51}); juce::MemoryBlock tremData; trem->getStateInformation(tremData);
    chainRecall->setStateInformation(tremData.getData(),static_cast<int>(tremData.getSize()));
    if(chainRecall->getChain()!=SpectralStripProcessor::Chain{0x52,0x51}
       || std::abs(chainRecall->state.getRawParameterValue(SpectralStripProcessor::moduleParamID(0,"tremoloRate"))->load()-2)>.01f
       || std::abs(chainRecall->state.getRawParameterValue(SpectralStripProcessor::moduleParamID(1,"tremoloRate"))->load()-7)>.01f
       || std::abs(chainRecall->state.getRawParameterValue(SpectralStripProcessor::moduleParamID(1,"tremoloShape"))->load()-.8f)>.01f) throw std::runtime_error("Tremolo independent recall failed");
    chainRecall->prepareToPlay(48000,257); float tremMin=1,tremMax=0;
    for(int block=0;block<400;++block)
    {
        for(int ch=0;ch<2;++ch) for(int i=0;i<257;++i) blurAudio.setSample(ch,i,.2f);
        chainRecall->processBlock(blurAudio,blurMidi);
        if(block>200) for(int i=0;i<257;++i)
        { const float sample=blurAudio.getSample(0,i); if(!std::isfinite(sample)) throw std::runtime_error("Tremolo processor nonfinite"); tremMin=std::min(tremMin,sample); tremMax=std::max(tremMax,sample); }
    }
    if(tremMax-tremMin<.15f) throw std::runtime_error("Tremolo controls did not reach audio");
    trem.reset();
    std::cout << "PASS processor Tremolo independent state and audio\n";
    auto* choice = processor->state.getParameter("fftSize");
    if (choice == nullptr) throw std::runtime_error("Missing FFT parameter");
    processor->prepareToPlay(48000, 257);
    juce::AudioBuffer<float> buffer(2,257);
    juce::MidiBuffer midi;
    for (int index : {0,4,1,3,2})
    {
        choice->setValueNotifyingHost(choice->convertTo0to1(static_cast<float>(index)));
        for (int block=0; block<64; ++block)
        {
            for(int ch=0; ch<2; ++ch)
                for(int i=0; i<257; ++i) buffer.setSample(ch,i,.1f);
            processor->processBlock(buffer,midi);
            for(int ch=0; ch<2; ++ch)
                for(int i=0; i<257; ++i)
                    if(!std::isfinite(buffer.getSample(ch,i))) throw std::runtime_error("Non-finite FFT transition");
        }
        const int expected=1 << (SpectralEngine::minOrder+index);
        if(processor->getLatencySamples()!=expected || processor->activeFFTSize.load()!=expected)
            throw std::runtime_error("Host latency not updated after FFT change");
        if(std::abs(buffer.getSample(0,256)-.1f)>1e-5f)
            throw std::runtime_error("Signal did not recover after FFT transition");
    }
    choice->setValueNotifyingHost(1);
    juce::MemoryBlock saved;
    processor->getStateInformation(saved);
    auto restored=std::make_unique<SpectralStripProcessor>();
    restored->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
    restored->prepareToPlay(48000,257);
    if(restored->getLatencySamples()!=8192) throw std::runtime_error("FFT state recall failed");
    auto oldState=processor->state.copyState();
    oldState.removeChild(oldState.getChildWithProperty("id","fftSize"),nullptr);
    juce::MemoryBlock oldData;
    auto xml=oldState.createXml(); juce::AudioProcessor::copyXmlToBinary(*xml,oldData);
    restored->setStateInformation(oldData.getData(),static_cast<int>(oldData.getSize()));
    restored->prepareToPlay(48000,257);
    if(restored->getChain()!=SpectralStripProcessor::Chain{0x11,0x22,0x33}) throw std::runtime_error("Legacy module order failed");
    if(restored->getLatencySamples()!=2048) throw std::runtime_error("Legacy state default is not 2048");
    auto preTilt=processor->state.copyState();
    for (const char* id : {"shimmer","gateAttack","gateRelease","gateTilt","damping"})
        preTilt.removeChild(preTilt.getChildWithProperty("id",id),nullptr);
    juce::MemoryBlock preTiltData;
    auto preTiltXml=preTilt.createXml(); juce::AudioProcessor::copyXmlToBinary(*preTiltXml,preTiltData);
    restored->setStateInformation(preTiltData.getData(),static_cast<int>(preTiltData.getSize()));
    const auto raw=[&](const char* id) { return restored->state.getRawParameterValue(id)->load(); };
    const auto near=[&](const char* id, float v) { return std::abs(raw(id)-v)<.01f; };
    if(!near("gateTilt",0) || !near("gateAttack",35) || !near("gateRelease",35) || !near("shimmer",0) || !near("damping",0))
        throw std::runtime_error("Pre-0.2 sessions do not keep their original gate/freeze/delay sound");
    // Rapid changes followed by host bypass must still settle and update latency.
    for(int block=0;block<80;++block)
    {
        choice->setValueNotifyingHost(block<20 ? static_cast<float>(block%5)/4 : 0);
        buffer.clear(); processor->processBlockBypassed(buffer,midi);
    }
    if(processor->getLatencySamples()!=512) throw std::runtime_error("Rapid FFT changes did not settle in bypass");
    // A non-zero first sample used to emerge as an abrupt step after the FFT latency.
    // Test onset, restart without prepareToPlay, seeks, host reset and continuous playback.
    for (int fftChoice=0; fftChoice<5; ++fftChoice)
    for (bool dryOnly : {false, true})
    {
        TestTransport transport;
        auto tested = std::make_unique<SpectralStripProcessor>();
        tested->setPlayHead(&transport);
        auto* fftParameter=tested->state.getParameter("fftSize");
        fftParameter->setValueNotifyingHost(fftParameter->convertTo0to1(static_cast<float>(fftChoice)));
        tested->state.getParameter("mix")->setValueNotifyingHost(dryOnly ? 0.0f : 1.0f);
        tested->prepareToPlay(48000,64);
        juce::AudioBuffer<float> audio(2,64);
        juce::int64 timeline=0;
        float previous=0;
        auto run = [&](int blocks, bool playing)
        {
            for(int block=0;block<blocks;++block)
            {
                transport.info.setIsPlaying(playing); transport.info.setTimeInSamples(timeline);
                for(int ch=0;ch<2;++ch) for(int i=0;i<64;++i) audio.setSample(ch,i,.5f);
                tested->processBlock(audio,midi);
                for(int i=0;i<64;++i)
                {
                    const float sample=audio.getSample(0,i);
                    if(!std::isfinite(sample) || std::abs(sample-previous)>.005f)
                        throw std::runtime_error("Discontinuity at transport start/seek/reset");
                    previous=sample;
                }
                if(playing) timeline+=64;
            }
        };
        run(160,true);
        if(std::abs(previous-.5f)>1e-5f) throw std::runtime_error("Startup fade never recovered");
        run(1,false); timeline=0; run(160,true);
        timeline=0; run(160,true);
        tested->reset(); run(160,true);
        if(std::abs(previous-.5f)>1e-5f) throw std::runtime_error("Transport reset muted continuous playback");
        tested->setPlayHead(nullptr);
    }
    std::cout << "PASS click regression: all FFT sizes, wet/dry, Play restart, seek and host reset\n";
    // LOW/HIGH must reach the engine through the parameters, with all effects off.
    auto processedEnergy = [&](float frequency, float low, float high, bool filter)
    {
        auto tested = std::make_unique<SpectralStripProcessor>();
        auto set = [&](const char* id, float v)
        { auto* p=tested->state.getParameter(id); p->setValueNotifyingHost(p->convertTo0to1(v)); };
        set("low",low); set("high",high); set("bandFilter",filter ? 1.0f : 0.0f);
        tested->prepareToPlay(48000,512);
        juce::AudioBuffer<float> audio(2,512);
        double energy=0; int t=0;
        for(int block=0;block<94;++block)
        {
            for(int i=0;i<512;++i,++t)
                for(int ch=0;ch<2;++ch)
                    audio.setSample(ch,i,.2f*std::sin(juce::MathConstants<float>::twoPi*frequency*static_cast<float>(t)/48000.0f));
            tested->processBlock(audio,midi);
            if(block>=47) for(int i=0;i<512;++i) energy+=audio.getSample(0,i)*audio.getSample(0,i);
        }
        return energy;
    };
    const double open=processedEnergy(1000,20,20000,true);
    if(processedEnergy(8000,20,3000,true)>open*.001) throw std::runtime_error("Processor HIGH did not filter");
    if(processedEnergy(1000,3000,20000,true)>open*.001) throw std::runtime_error("Processor LOW did not filter");
    if(processedEnergy(1000,20,3000,true)<open*.95) throw std::runtime_error("Processor filter damaged passband");
    if(processedEnergy(8000,20,3000,false)<open*.95) throw std::runtime_error("Processor FILTER bypass failed");
    std::cout << "PASS processor LOW/HIGH filter through parameters\n";
    // Topology edits during playback must settle back to finite, full-level audio.
    auto topology=std::make_unique<SpectralStripProcessor>(); topology->prepareToPlay(48000,257);
    for (const auto chain : {SpectralStripProcessor::Chain{0x33,0x11,0x22},SpectralStripProcessor::Chain{0,0,0,0},SpectralStripProcessor::Chain{0x22,0x33,0x11}})
    {
        topology->setChain(chain);
        double energy=0;
        for(int block=0;block<100;++block)
        {
            for(int i=0;i<257;++i) for(int ch=0;ch<2;++ch) buffer.setSample(ch,i,.1f);
            topology->processBlock(buffer,midi);
            for(int i=0;i<257;++i)
            {
                const float sample=buffer.getSample(0,i);
                if(!std::isfinite(sample) || std::abs(sample)>.2f) throw std::runtime_error("Unstable module transition");
                if(block>80) energy+=sample*sample;
            }
        }
        if(energy<40) throw std::runtime_error("Module transition failed to recover audio");
    }
    // Exercise the real processor (no test-only allocation in its DSP path).
    {
        auto live = std::make_unique<SpectralStripProcessor>();
        live->prepareToPlay(48000, 128);
        std::unique_ptr<juce::AudioProcessorEditor> editor(live->createEditor());
        juce::ComboBox* add=nullptr;
        for(int i=0;i<editor->getNumChildComponents();++i)
            if(auto* combo=dynamic_cast<juce::ComboBox*>(editor->getChildComponent(i)))
                if(combo->getTextWhenNothingSelected()=="+  Add module") add=combo;
        if(add==nullptr) throw std::runtime_error("Missing add-module menu");
        std::atomic<bool> running {true}, finite {true};
        std::atomic<int> blocks {0};
        std::thread audio([&]
        {
            juce::AudioBuffer<float> block(2, 128);
            juce::MidiBuffer events;
            while (running.load())
            {
                for (int ch=0; ch<2; ++ch)
                    for (int n=0; n<128; ++n) block.setSample(ch,n,.05f);
                live->processBlock(block, events);
                for (int ch=0; ch<2; ++ch)
                    for (int n=0; n<128; ++n)
                        if (!std::isfinite(block.getSample(ch,n))) finite.store(false);
                ++blocks;
            }
        });
        for (int pass=0; pass<12; ++pass)
        {
            live->setChain({});
            for (int slot=0; slot<SpectralEngine::maxModules; ++slot)
                add->setSelectedId(1+(slot+pass)%6,juce::sendNotificationSync);
            editor->setSize(pass%2==0 ? 780 : 1040,pass%2==0 ? 600 : 800);
            editor->createComponentSnapshot(editor->getLocalBounds());
            auto chain=live->getChain();
            std::reverse(chain.begin(),chain.end()); live->setChain(chain);
            live->reset();
            if (pass%4==0) live->prepareToPlay(pass%8==0 ? 96000 : 48000,128);
            juce::MemoryBlock saved; live->getStateInformation(saved);
            live->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
        }
        running.store(false); audio.join();
        if (!finite.load() || blocks.load()==0) throw std::runtime_error("Concurrent chain edits failed");
        live->setChain({});
        juce::AudioBuffer<float> block(2,128); juce::MidiBuffer events;
        double energy=0;
        for(int b=0;b<1000;++b)
        {
            for(int ch=0;ch<2;++ch) for(int n=0;n<128;++n) block.setSample(ch,n,.05f);
            live->processBlock(block,events);
            if(b>950) for(int n=0;n<128;++n) energy+=block.getSample(0,n)*block.getSample(0,n);
        }
        if(energy<1) throw std::runtime_error("Audio did not recover after concurrent edits");
        std::cout << "PASS concurrent insert/remove/reorder, state recall and reprepare during audio\n";
    }
    std::cout << "PASS module chain recall, legacy migration and playback transitions\n";
    std::cout << "PASS processor FFT transitions, latency updates, state recall, legacy state and host bypass\n";
}
