

#### Setting up the environment


On a fresh PSoC Creator installation the CYBLE-416045-02 device components are not
present, so they must be added via Tools > Find new devices. The default PDL version (as of May 2019) does not support it either, so the PDL 3.0.4 and up (3.1 being the latest/last version) needs to be downloaded and installed from either the update manager or Cypress website.

For the DSP library, the core CMSIS and arm_math headers are automatically generated and present in Generated_Sources/PSoC6/pdl/cmsis/include. Only the C files needed for a specific filter have to be copied from the CMSIS-DSP library to the project (arm_fir_init_q15.c, arm_fir_q15.c, etc...).

The last thing to do is to add defines for ARM_MATH_CM4 and __FPU_PRESENT=1 to the compiler command line in the Build Settings. Defining them directly in arm_math.h is not recommended since it is regenerated every time a change is made in the TopDesign.

![buildsettings](https://user-images.githubusercontent.com/36741050/58102662-50d1c800-7bea-11e9-9035-618d4437cb4f.png)


---

#### Hardware Configuration

For experimenting with filters I found more convenient to process an audio signal in real time, since it can be 1-heard and 2-visualized & imported into Matlab easily with the help of a logic analyzer. Since my function generators seemed unreliable under 5 volts, I chose to use a programmable sound generator (PSG) instead. 

Internally, on the analog side the device has a 12-bit ADC, a 12-bit DAC and 2 op-amps. Since my PSG signal goes from 0 to 0.65 volt I used an op-amp with a gain of 2 to the ADC, and the second op-amp as an output buffer for the DAC. The ADC uses the system bandgap (1.2V) as vref and outputs signed values calculated on the range from 0 to 2*vref. In practice it means that the signal is using half the resolution at best. On the bright side it reduces the risk of overflow and clipping on the output buffer, and is fine for experimentations.

![topdesign](https://user-images.githubusercontent.com/36741050/58102535-18ca8500-7bea-11e9-997d-bf181c315149.png)

The processing is done in blocks of 256 samples. Chained DMA descriptors are used on the ADC and DAC to alternate between 3 buffers. An interrupt triggers on the ADC descriptor completion to signal to the main program that a buffer is ready to be processed.

![buffering](https://user-images.githubusercontent.com/36741050/58102317-c5f0cd80-7be9-11e9-9879-7c2bb90be18a.png)

---

#### Filters

##### FIR

CMSIS-DSP files used:
- arm_fir_init_q15.c
- arm_fir_q15.c

The filters are designed with Matlab and the CMSIS-DSP is used for now to test the filters. Minors modifications were made to the FIR filter functions to handle the samples. 

An offset of 1024 is added to the samples when they are loaded into the state buffer to avoid undershoot problems
```C
// *__SIMD32(pStateCurnt)++ = *__SIMD32(pSrc)++; Was replaced by
*__SIMD32(pStateCurnt)++ = __SADD16(*__SIMD32(pSrc)++, 0x04000400);
```
and the results are saturated on 12 bits instead of 16.

To test the filters, both the input and output signals are captured with a logic analyzers, then imported into Matlab. As an example, an equiripple with -30dB on the stopband, set at 5kHz, made for the sake of visualization:

![figure1](https://user-images.githubusercontent.com/36741050/58102292-bd989280-7be9-11e9-8a83-2c2d528fea4b.png)
*Square wave semi-tone sweep from 0 to 8000Hz*

Of course the harmonics are all over the place and the lack of anti-aliasing filter is apparent. With a more complex signal the filter can be seen into action: filtering out most of the noise, removing the high frequency components and rounding the mid-frequency square waves by filtering out their higher harmonics:

![figure2](https://user-images.githubusercontent.com/36741050/58102297-bffaec80-7be9-11e9-8a0f-a2a37d78e046.png)
*Bottom: Input - Top: filtered signal
---
##### IIR SOS

CMSIS-DSP files used:
- arm_biquad_cascade_df1_init_q15.c
- arm_biquad_cascade_df1_q15.c

There are a couple pitfalls with the IIR filters.

First, I have yet to see a filter that can be made stable with only a sign and 15 fractional bits. Using at least q1.14 seems inevitable in most of the case. A postshift of 1 should be set in the init function for this and both the denominator and nominator coefficients should have the same amount of fractional bits.  Another problem comes from the sign of the denominators. To successfully import a design from Matlab, the signs have to be flipped.

I made a small function to convert the coefficients from the Matlab header to the format used by the CMSIS library and could not adjust the library function for my offset, so I added it to my buffer in a loop before calling the filter function on it. 

Again, with a low-pass filter I had similar results.
![iirfig1](https://user-images.githubusercontent.com/36741050/58102249-a9ed2c00-7be9-11e9-99e8-3ccb73883ed8.png)
![iirfig2](https://user-images.githubusercontent.com/36741050/58102276-b3769400-7be9-11e9-9934-290ffd9d9c87.png)

