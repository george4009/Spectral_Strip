#include "../Source/PluginProcessor.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
void runSmearTiltTests()
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto processor=std::make_unique<SpectralStripProcessor>();
    processor->setChain({0x22});
    auto set=[&](const juce::String& id,float value) {
        auto* p=processor->state.getParameter(id);
        if(!p) throw std::runtime_error("Missing tone parameter");
        p->setValueNotifyingHost(p->convertTo0to1(value));
    };
    const auto tiltID=SpectralStripProcessor::moduleParamID(1,"toneTilt");
    set("gate",0);set("bandFilter",0);
    auto energy=[&](float frequency,float tilt,int choice,bool selectedBand=false) {
        set(tiltID,tilt);set("fftSize",static_cast<float>(choice));
        set("low",selectedBand ? 2000.0f : 20.0f);set("high",selectedBand ? 8000.0f : 20000.0f);
        processor->prepareToPlay(48000,256);
        juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;
        double sum=0;
        for(int block=0;block<250;++block) {
            for(int i=0;i<256;++i) {
                audio.setSample(0,i,.02f*std::sin(juce::MathConstants<double>::twoPi*frequency*(block*256+i)/48000));
                audio.setSample(1,i,0);
            }
            processor->processBlock(audio,midi);
            for(int i=0;i<256;++i) {
                if(!std::isfinite(audio.getSample(0,i)) || std::abs(audio.getSample(1,i))>1e-8f)
                    throw std::runtime_error("Tone tilt produced invalid audio/stereo leakage");
                if(block>=150) sum+=audio.getSample(0,i)*audio.getSample(0,i);
            }
        }
        return sum;
    };
    for(int choice: {0,2,4}) for(float frequency: {250.0f,1000.0f,4000.0f}) {
        const double reference=energy(frequency,0,choice);
        for(float tilt: {-3.0f,3.0f}) {
            const double gain=10*std::log10(energy(frequency,tilt,choice)/reference);
            const double expected=tilt*std::log2(frequency/1000);
            std::cout<<"Tone tilt FFT "<<(1<<(9+choice))<<" frequency "<<frequency<<" tilt "<<tilt<<" gain "<<gain<<" dB\n";
            if(std::abs(gain-expected)>1.5) throw std::runtime_error("Tone tilt response differs from intended slope");
        }
    }
    if(std::abs(10*std::log10(energy(250,6,2,true)/energy(250,0,2,true)))>.1)
        throw std::runtime_error("Tone tilt changed audio outside the selected band");
    set(tiltID,-2.25f);
    const auto oldRange=processor->state.getParameter("gateTilt")->getNormalisableRange();
    if(oldRange.start!=0 || oldRange.end!=6) throw std::runtime_error("Legacy gate tilt range changed");
    juce::MemoryBlock saved;processor->getStateInformation(saved);
    auto recalled=std::make_unique<SpectralStripProcessor>();recalled->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
    if(std::abs(recalled->state.getRawParameterValue(tiltID)->load()+2.25f)>.001f)
        throw std::runtime_error("Tone tilt state did not recall");
    // Simulate an old state with no tone parameters: the new EQ must be neutral.
    auto tree=processor->state.copyState();
    for(int i=tree.getNumChildren();--i>=0;) if(tree.getChild(i).getProperty("id").toString().endsWith("_toneTilt")) tree.removeChild(i,nullptr);
    auto xml=tree.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,saved);
    recalled->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
    if(std::abs(recalled->state.getRawParameterValue(tiltID)->load())>.001f)
        throw std::runtime_error("Legacy state inherited a non-neutral tone tilt");
    std::cout<<"PASS audible bipolar tone tilt with Gate off, pivot, band isolation, stereo and state compatibility\n";
}
