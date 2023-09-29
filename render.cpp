/*
 ____  _____ _        _    
| __ )| ____| |      / \   
|  _ \|  _| | |     / _ \  
| |_) | |___| |___ / ___ \ 
|____/|_____|_____/_/   \_\

http://bela.io

C++ Real-Time Audio Programming with Bela - Lecture 11: Circular buffers
circular-buffer: template code for implementing delays
*/

#include <Bela.h>
#include <vector>
#include "MonoFilePlayer.h"
#include <libraries/Gui/Gui.h>
#include <libraries/GuiController/GuiController.h>
#include <libraries/Scope/Scope.h>
#include <libraries/math_neon/math_neon.h>
#include <cmath>
#include "Wavetable.h"

// Name of the sound file (in project folder)
std::string gFilename = "slow-drum-loop.wav";

// Object that handles playing sound from a buffer
MonoFilePlayer gPlayer;

// TODO: declare variables for circular buffers for each branch
std::vector<float> CircularBuffer_in, CircularBuffer_out, CircularBuffer_tmp, CircularBuffer_tmp2, CircularBuffer_lpf;
std::vector<float> Buffer_in_2, Buffer_out_2, Buffer_tmp_2, Buffer_tmp2_2, Buffer_lpf_2;
std::vector<float> Buffer_in_3, Buffer_out_3, Buffer_tmp_3, Buffer_tmp2_3, Buffer_lpf_3;
std::vector<float> Buffer_in_4, Buffer_out_4, Buffer_tmp_4, Buffer_tmp2_4, Buffer_lpf_4;

// unsigned int pointer for each branch;
unsigned int ReadPointer, ReadPointer_2, ReadPointer_3, ReadPointer_4, WritePointer;
// unsigned int value for delay-line for each branch'
unsigned int delay = 0;
unsigned int delay_2 = 0;
unsigned int delay_3 = 0;
unsigned int delay_4 = 0;

// definition of GUI
Gui gGui;
GuiController gGuiController;

// define the supportings for BELA
const int kButtonPin = 1;				
int gLastButtonStatus = HIGH;

// Browser-based oscilloscope
Scope gScope;
// Oscillator objects
Wavetable gSineOscillator, sin2, sin3, sin4;


bool setup(BelaContext *context, void *userData)
{
	// Load the audio file
	if(!gPlayer.setup(gFilename)) {
    	rt_printf("Error loading audio file '%s'\n", gFilename.c_str());
    	return false;
	}
	// Initialise circular buffers in delay pointers
	CircularBuffer_in.resize(0.3 * context->audioSampleRate);
	CircularBuffer_tmp.resize(0.3 * context->audioSampleRate);
	CircularBuffer_tmp2.resize(0.3 * context->audioSampleRate);
	CircularBuffer_lpf.resize(0.3 * context->audioSampleRate);
	CircularBuffer_out.resize(0.3 * context->audioSampleRate);
	Buffer_in_2.resize(0.3 * context->audioSampleRate);
	Buffer_out_2.resize(0.3 * context->audioSampleRate);
	Buffer_tmp_2.resize(0.3 * context->audioSampleRate);
	Buffer_tmp2_2.resize(0.3 * context->audioSampleRate);
	Buffer_lpf_2.resize(0.3 * context->audioSampleRate);
		Buffer_in_3.resize(0.3 * context->audioSampleRate);
	Buffer_out_3.resize(0.3 * context->audioSampleRate);
	Buffer_tmp_3.resize(0.3 * context->audioSampleRate);
	Buffer_tmp2_3.resize(0.3 * context->audioSampleRate);
	Buffer_lpf_3.resize(0.3 * context->audioSampleRate);
		Buffer_in_4.resize(0.3 * context->audioSampleRate);
	Buffer_out_4.resize(0.3 * context->audioSampleRate);
	Buffer_tmp_4.resize(0.3 * context->audioSampleRate);
	Buffer_tmp2_4.resize(0.3 * context->audioSampleRate);
	Buffer_lpf_4.resize(0.3 * context->audioSampleRate);

	//Gui setup
	gGui.setup(context->projectName);
	gGuiController.setup(&gGui,"Delay Controller");
	//Gui arguement, default value, min, max, increasement
	gGuiController.addSlider("Delay", 0.01, 0.0, 0.05, 0); // upto 1k samples / 22.05kHz
	gGuiController.addSlider("Feadback", 0.5, 0, 0.95, 0);
	gGuiController.addSlider("LPF feedback b", 0.5, 0, 0.95, 0);
	gGuiController.addSlider("comb filter freq Hz", 0.0, 0, 20, 0);
	// gGuiController.addSlider("comb filter amplitude", 0.0, 0, 0.9, 0);
	gGuiController.addSlider("Delay_2", 0.01, 0.0, 0.05, 0); // upto 1k samples / 22.05kHz
	gGuiController.addSlider("comb filter freq_2 Hz", 0.0, 0, 20, 0);
	gGuiController.addSlider("Delay_3", 0.01, 0.0, 0.05, 0); // upto 1k samples / 22.05kHz
	gGuiController.addSlider("comb filter freq_3 Hz", 0.0, 0, 20, 0);
	gGuiController.addSlider("Delay_4", 0.01, 0.0, 0.05, 0); // upto 1k samples / 22.05kHz
	gGuiController.addSlider("comb filter freq_4 Hz", 0.0, 0, 20, 0);
	
	gGuiController.addSlider("global feedback beta", 0.0, 0, 0.95, 0);
	gGuiController.addSlider("output ratio c", 0.0, 0, 0.95, 0);

	
	// Print some useful info
    rt_printf("Loaded the audio file '%s' with %d frames (%.1f seconds)\n", 
    			gFilename.c_str(), gPlayer.size(),
    			gPlayer.size() / context->audioSampleRate);
    
    // set up the button
    pinMode(context, 0, kButtonPin, INPUT);
    
    // Set up the oscilloscope
	gScope.setup(1, context->audioSampleRate);
	// wavetable for comb filter parameters modulation
	std::vector<float> wavetable;
	const unsigned int wavetableSize = 1024;
	// Populate a buffer with the first 64 harmonics of a sawtooth wave
	wavetable.resize(wavetableSize);
	// Recalculate the wavetable for a sine
	for(unsigned int n = 0; n < wavetableSize; n++) {
		wavetable[n] = sin(2.0 * M_PI * (float)n / (float)wavetableSize);
	}	
	// Initialise the sine oscillator
	gSineOscillator.setup(context->audioSampleRate, wavetable);

	return true;
}

void render(BelaContext *context, void *userData)
{	
	// Keep track of whether to play input with reverbration or not
	static bool playReveb = false; 
    
    // delay and ReadPointers for each branch
    float float_delay = gGuiController.getSliderValue(0);
    delay = float_delay * context-> audioSampleRate;
    ReadPointer = (WritePointer - delay + CircularBuffer_out.size()) % CircularBuffer_out.size();
    float feadback = gGuiController.getSliderValue(1);
	float b = gGuiController.getSliderValue(2);
	float freq = gGuiController.getSliderValue(3);
		float float_delay_2 = gGuiController.getSliderValue(4);
	delay_2 = float_delay_2 * context-> audioSampleRate;
    ReadPointer_2 = (WritePointer - delay_2 + CircularBuffer_out.size()) % CircularBuffer_out.size();
	float freq_2 = gGuiController.getSliderValue(5);
		float float_delay_3 = gGuiController.getSliderValue(6);
	delay_3 = float_delay_3 * context-> audioSampleRate;
    ReadPointer_3 = (WritePointer - delay_3 + CircularBuffer_out.size()) % CircularBuffer_out.size();
	float freq_3 = gGuiController.getSliderValue(7);
		float float_delay_4 = gGuiController.getSliderValue(8);
	delay_4 = float_delay_4 * context-> audioSampleRate;
    ReadPointer_4 = (WritePointer - delay_4 + CircularBuffer_out.size()) % CircularBuffer_out.size();
	float freq_4 = gGuiController.getSliderValue(9);
	float beta = gGuiController.getSliderValue(10);
	float c = gGuiController.getSliderValue(11);
	
	// set up the LFO for the modulation of comb filters' parameters
	gSineOscillator.setFrequency(freq);
	sin2.setFrequency(freq_2);
	sin3.setFrequency(freq_3);
	sin4.setFrequency(freq_4);
	
    for(unsigned int n = 0; n < context->audioFrames; n++) {
        float in = gPlayer.process();
        float out = 0.0f;
        
        // get the button status
        int status = digitalRead(context, n, kButtonPin);
        if(status == LOW && gLastButtonStatus == HIGH) {
            playReveb = !playReveb; // Toggle the state of playInput
            rt_printf("playReveb = %d\n", playReveb);
        }
        gLastButtonStatus = status;
        
        // get the comb filter's feed back value
        float a = 0.9 * gSineOscillator.process();
        float a_2 = 0.9 * sin2.process();
        float a_3 = 0.9 * sin3.process();
        float a_4 = 0.9 * sin4.process();
        
		//Global feedback outside the four branches
		float sum = beta * (CircularBuffer_out[WritePointer-1] + Buffer_out_3[WritePointer-1] + Buffer_out_2[WritePointer-1] + Buffer_out_4[WritePointer-1]);
    	// implement a delay-line and a first-order low-pass filter to change CircularBuffer_in to CircularBuffer_ou
		CircularBuffer_in[WritePointer] = in + sum ;
		CircularBuffer_tmp[WritePointer] = in + feadback * CircularBuffer_out[ReadPointer];
		CircularBuffer_lpf[WritePointer] =(1/(1+b)) * ( CircularBuffer_tmp[WritePointer] + b * CircularBuffer_lpf[WritePointer-1]);
		CircularBuffer_out[WritePointer] = CircularBuffer_lpf[ReadPointer] + a * CircularBuffer_out[ReadPointer] - a * CircularBuffer_lpf[WritePointer];
		// implementing the samething for all the other branches
			Buffer_in_2[WritePointer] = in + sum ;
		Buffer_tmp_2[WritePointer] = in + feadback * Buffer_out_2[ReadPointer_2];
		Buffer_lpf_2[WritePointer] =(1/(1+b)) * ( Buffer_tmp_2[WritePointer] + b * Buffer_lpf_2[WritePointer-1]);
		Buffer_out_2[WritePointer] = Buffer_lpf_2[ReadPointer_2] + a_2 * Buffer_out_2[ReadPointer_2] - a_2 * Buffer_lpf_2[WritePointer];
			Buffer_in_3[WritePointer] = in + sum ;
		Buffer_tmp_3[WritePointer] = in + feadback * Buffer_out_3[ReadPointer_3];
		Buffer_lpf_3[WritePointer] =(1/(1+b)) * ( Buffer_tmp_3[WritePointer] + b * Buffer_lpf_3[WritePointer-1]);
		Buffer_out_3[WritePointer] = Buffer_lpf_3[ReadPointer_3] + a_3 * Buffer_out_3[ReadPointer_3] - a_3 * Buffer_lpf_3[WritePointer];
			Buffer_in_4[WritePointer] = in + sum ;
		Buffer_tmp_4[WritePointer] = in + feadback * Buffer_out_4[ReadPointer_4];
		Buffer_lpf_4[WritePointer] =(1/(1+b)) * ( Buffer_tmp_4[WritePointer] + b * Buffer_lpf_4[WritePointer-1]);
		Buffer_out_4[WritePointer] = Buffer_lpf_4[ReadPointer_4] + a_4 * Buffer_out_4[ReadPointer_4] - a_4 * Buffer_lpf_4[WritePointer];
		
		// evluate linear combination of each branch to get the output
        float left =0.0;
        float right = 0.0;
        if(playReveb) {
			left = CircularBuffer_out[WritePointer] - Buffer_out_3[WritePointer];
    		right = Buffer_out_2[WritePointer] - Buffer_out_4[WritePointer];
    		left *= c;
    		right *= c;
    	}
    	else{
    		left = in;
    		right = in;
    	}
        
        // update readpointers and WritePointer
        ReadPointer++;
        if (ReadPointer >= CircularBuffer_in.size()){
        	ReadPointer = 0;
        }
        ReadPointer_2++;
        if (ReadPointer_2 >= CircularBuffer_in.size()){
        	ReadPointer_2 = 0;
        }
        ReadPointer_3++;
        if (ReadPointer_3 >= CircularBuffer_in.size()){
        	ReadPointer_3 = 0;
        }
        ReadPointer_4++;
        if (ReadPointer_4 >= CircularBuffer_in.size()){
        	ReadPointer_4 = 0;
        }
        WritePointer++;
        if (WritePointer >= CircularBuffer_in.size()){
        	WritePointer = 0;
        }
        
		// Write the input and output to different channels
    	// for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
    	// 	audioWrite(context, n, channel, out);
    	// }
    	audioWrite(context, n, 0, left);
    	audioWrite(context, n, 1, right);
    	audioWrite(context, n, 2, left);
    	audioWrite(context, n, 3, right);
    	gScope.log(left+right);  
    }
}

void cleanup(BelaContext *context, void *userData)
{

}
