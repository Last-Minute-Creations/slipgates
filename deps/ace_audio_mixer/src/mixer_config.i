; $VER: mixer_config.i 3.1 (10.03.23)
;
; mixer_config.i
; Configuration file for the audio mixer.
;
; For mixer API, see mixer.i and the rest of the mixer documentation.
; For more information about this configuration file, see the
; mixer documentation.
;
; Note: the audio mixer expects samples to be pre-processed such that adding
;       sample values together for all mixer channels can never exceed 8 bit
;       signed limits or overflow from positive to negative (or vice versa).
;
; Author: Jeroen Knoester
; Version: 3.1
; Revision: 20230310
;
; Assembled using VASM in Amiga-link mode.
; TAB size = 4 spaces

	IFND	MIXER_CONFIG_I
MIXER_CONFIG_I	SET	1

; Configuration defines
;-----------------------------------------------------------------------------
; Mixer type selection
;-----------------------------------------------------------------------------
; Set one of the defines below to 1 to select the desired mixer type:
; (note that only the code required for the selected type will end up in the
;  object file, with specific optimisations for each type as required)
;
; * MIXER_SINGLE		- mixer outputs to a single hardware channel.
;
; * MIXER_MULTI 		- mixer outputs to multiple hardware channels.
;
;						  Each hardware channel gets its own set of software
;						  mixed channels.
;
; * MIXER_MULTI_PAIRED	- mixer outputs to multiple hardware channels.
;
;						  Hardware channels are split into one pair of
;						  channels and between zero and two unpaired channels.
;
;						  The paired hardware channels both play back the same
;						  set of software mixed channels. The unpaired channels
;						  (if any) each get their own set of software mixed
;						  channels.
;
;						  The paired channels will always be AUD2 & AUD3.
;						  Unpaired channels are either none, AUD0, AUD1 or
;						  AUD0 & AUD1.
MIXER_SINGLE			EQU 1
MIXER_MULTI				EQU	0
MIXER_MULTI_PAIRED		EQU 0


;-----------------------------------------------------------------------------
; Mixer output configuration
;-----------------------------------------------------------------------------
mixer_output_channels	EQU	DMAF_AUD3
									; valid values are DMAF_AUD0/1/2/3 or
									; any bitwise combination of these with at
									; least one channel set.
									;
									; Note: combining channels is only
									;       valid when MIXER_MULTI or
									;       MIXER_MULTI_PAIRED is set.
									; Note: if MIXER_MULTI_PAIRED is set,
									;       valid combinations are:
									;       * DMAF_AUD2|DMAF_AUD3
									;       * DMAF_AUD2|DMAF_AUD3|DMAF_AUD0
									;       * DMAF_AUD2|DMAF_AUD3|DMAF_AUD1
									;		* all 4 channels

mixer_sw_channels		EQU	2		; Maximum number of software mixed
									; channels on a single hardware channel.
									; Valid values are 1,2,3 and 4
mixer_period			EQU	161 	; Valid values: 124 and up
									; Default value of 322 is about
									; 11025Hz when using a PAL Amiga
									;
									; Note: there are minor differences in
									;       period values between PAL and NTSC
									;       for the same desired output
									;       frequency.
									;
									;       These differences are not taken
									;       into account, the value set here
									;       is used as the actual HW period
									;       value for the mixer, irrespective
									;       of the video system of the Amiga
									;       the mixer is running on.

MIXER_PER_IS_NTSC		EQU	0		; Set to 1 if the mixer period value set
									; above is given for NTSC systems, leave
									; at 0 if it is given for PAL systems.


;-----------------------------------------------------------------------------
; Optimisation related options
;-----------------------------------------------------------------------------
; Set define below to 1 to optimise mixer code for the 68020+.
; If set to 0, optimised code for 68000 is used instead.
;
; Note: code generated will run on 68000/68010 and 68020+ irrespective of this
;       setting, the only difference is one of speed - the 68000 code is
;       significantly faster on 68000/68010 than the 68020+ code and
;       vice versa.
MIXER_68020				EQU 0

; Optimisations below only apply to the 68000 version, as they will not
; improve performance for 68020+ code/machines.

; Set define below to 1 to optimise the mixer by limiting maximum sample
; length to 65532 bytes (technically up to 65535, but the mixer always rounds
; lengths down to the nearest multiple of 4 bytes). This increases performance
; by switching some operations from being longword based to word based.
MIXER_WORDSIZED			EQU 0

; Set define below to 1 to optimise the mixer by forcing both the buffer size
; and source sample sizes to be multiples of 32 bytes. This improves
; performance slightly by removing the need to deal with samples that are a
; multiple of 4 bytes in size.
;
; Note: the amount of performance gained this way will be dependend on the
;       calculated buffer size for the chosen period - if this buffer size is
;       already a multiple of 32, the performance gain will be very limited or
;       non-existent.
; Note: irrespective of the calculated buffer size, the possible gain for this
;       option is usually limited.
MIXER_SIZEX32			EQU 0

; Set define below to 1 to optimise the mixer by forcing the source sample
; size to always be a multiple of the buffer size. This increases performance
; significantly by removing the need to decide the correct number of bytes to
; mix.
;
; Note: this setting adapts to the video system selected at runtime using the
;       MixerSetup() routine and will use an appropriate buffer size to keep
;       the interrupt running once per frame for both PAL and NTSC based
;       displays.
; Note: off all the performance options for the 68000 code, this will give the
;       largest performance gain, at the cost of needing (potentially
;       significantly) more memory for samples.
; Note: if both this and MIXER_SIZEX32 are set, the buffer size will be a
;       multiple of 32 bytes.
; Note: the combination of MIXER_SIZEX32 and MIXER_SIZEXBUF does not normally
;       increase performance meaningfully - selecting MIXER_SIZEXBUF will
;       mostly make MIXER_SIZEX32 irrelevant. However, in some edge cases,
;       it can still create a slight increase in performance over just using
;       MIXER_SIZEXBUF.
MIXER_SIZEXBUF			EQU 0


;-----------------------------------------------------------------------------
; Performance measurement options
;-----------------------------------------------------------------------------
; Set define below to 1 to enable colour/timing bars
MIXER_TIMING_BARS		EQU	1
; Set to desired background colour to reset to after colour bars end (valid if
; mixer timing bars are in use).
MIXER_DEFAULT_COLOUR	EQU	$000

; Set define below to 1 to enable CIA based timing of the performance of the
; interrupt handler. This uses the CIA-A timer.
MIXER_CIA_TIMER			EQU 0
; Set define below to 1 to enable restoring the keyboard for OS use after
; using CIA based performance timing. Has no function if MIXER_CIA_TIMER is
; set to 0.
MIXER_CIA_KBOARD_RES	EQU	1


;-----------------------------------------------------------------------------
; Advanced options
;-----------------------------------------------------------------------------
; Set define below to 1 to include the mixer in section code,code.
; If set to 0, the mixer will not be set a specific section (normally this is
; not needed, but it can be useful in certain cases)
MIXER_SECTION			EQU	1

; Set define below to 1 if the assembler used does not support the "echo"
; command. This blocks mixer.asm displaying messages during assembly.
MIXER_NO_ECHO			EQU 0
	ENDC	; MIXER_CONFIG_I

; Set define below to 1 to include C style function definition aliases in
; addition to the standard function definitions. This effectively creates
; a number of XDEF _<FunctionName> symbols for use in linking against C based
; code.
MIXER_C_DEFS			EQU 1
; End of File
