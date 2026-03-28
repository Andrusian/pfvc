# pfvc
PipeWire Fine Volume Control

I recently updated my Linux and was surprised to find that PulseAudio had been replaced by PipeWire. This offered some improvements considering that PulseAudio was
challenging to get working a few years ago. But I was unaware of this and the accidental mish-mash of the two caused system hangs and crashes. So PulseAudio and the 
PulseAudio Volume Control got purged from my system.

PipeWire works great but I noticed that Plasma desktop widgets have a few issues. In particular, I needed fine volume control. The Plasma widget for PipeWire has a bug: 
it does a drag even rather than moving the slider slowly. Yes, I could have used cursor controls, but this is a multi-window usage and it wasn't convenient.
And yes, I submitted a bug report on the problem.

What I wanted was a fine volume control with small increments and could handle several output devices at once. It should also allow remapping of application streams
to output.

After writing a thorough specification, the result was coded up in first Claude, then Gemini, then back to Claude. Let's clearly state that these two AIs *really* struggled
and it far exceeded their context windows. The result will be whatever winds up in this repository.

Use or adapt this code to whatever you need it for. PipeWire is a complex system and has some challenges you have to work through. Of particular note is that
external events affecting the audio system can occur at any time. This app has to deal with that as well is it's own user actions.
