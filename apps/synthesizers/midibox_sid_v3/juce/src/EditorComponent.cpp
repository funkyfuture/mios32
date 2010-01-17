/*
  ==============================================================================
  Derived from DemoEditorComponent.cpp of Juce project
 
 
  ==============================================================================
*/

#include "includes.h"
#include "EditorComponent.h"

//==============================================================================
// quick-and-dirty function to format a timecode string
static const String timeToTimecodeString (const double seconds)
{
  const double absSecs = fabs (seconds);
  const tchar* const sign = (seconds < 0) ? T("-") : T("");
  
  const int hours = (int) (absSecs / (60.0 * 60.0));
  const int mins  = ((int) (absSecs / 60.0)) % 60;
  const int secs  = ((int) absSecs) % 60;
  
  return String::formatted (T("%s%02d:%02d:%02d:%03d"),
			    sign, hours, mins, secs,
			    roundDoubleToInt (absSecs * 1000) % 1000);
}

// quick-and-dirty function to format a bars/beats string
static const String ppqToBarsBeatsString (const double ppq,
                                          const double lastBarPPQ,
                                          const int numerator,
                                          const int denominator)
{
  if (numerator == 0 || denominator == 0)
    return T("1|1|0");
  
  const int ppqPerBar = (numerator * 4 / denominator);
  const double beats  = (fmod (ppq, ppqPerBar) / ppqPerBar) * numerator;
  
  const int bar       = ((int) ppq) / ppqPerBar + 1;
  const int beat      = ((int) beats) + 1;
  const int ticks     = ((int) (fmod (beats, 1.0) * 960.0));
  
  String s;
  s << bar << T('|') << beat << T('|') << ticks;
  return s;
}


//==============================================================================
EditorComponent::EditorComponent (AudioProcessing* const ownerSidEmu)
: AudioProcessorEditor (ownerSidEmu)
{
  // create our gain slider..
  addAndMakeVisible (gainSlider = new Slider (T("gain")));
  gainSlider->addListener (this);
  gainSlider->setRange (0.0, 1.0, 0.01);
  gainSlider->setTooltip (T("changes the volume of the audio that runs through the plugin.."));
  gainSlider->setValue (ownerSidEmu->getParameter(0), false);
  
  // create our patch selection slider..
  addAndMakeVisible (patchSlider = new Slider (T("patch")));
  patchSlider->addListener (this);
  patchSlider->setRange (0.0, 127.0, 1.0);
  patchSlider->setTooltip (T("selects a patch"));
  patchSlider->setValue (ownerSidEmu->getParameter(2), false);
  
  // create and add the midi keyboard component..
  addAndMakeVisible (midiKeyboard
		     = new MidiKeyboardComponent (ownerSidEmu->keyboardState,
						  MidiKeyboardComponent::horizontalKeyboard));
  
  // add a label that will display the current timecode and status..
  addAndMakeVisible (infoLabel = new Label (String::empty, String::empty));
  
  // add the triangular resizer component for the bottom-right of the UI
  addAndMakeVisible (resizer = new ResizableCornerComponent (this, &resizeLimits));
  resizeLimits.setSizeLimits (150, 150, 800, 300);
  
  // set our component's initial size to be the last one that was stored in the SidEmu's settings
  setSize (ownerSidEmu->lastUIWidth,
	   ownerSidEmu->lastUIHeight);
  
  // register ourselves with the SidEmu - it will use its ChangeBroadcaster base
  // class to tell us when something has changed, and this will call our changeListenerCallback()
  // method.
  ownerSidEmu->addChangeListener (this);
}

EditorComponent::~EditorComponent()
{
  getSidEmu()->removeChangeListener (this);
  
  deleteAllChildren();
}

//==============================================================================
void EditorComponent::paint (Graphics& g)
{
  // just clear the window
  g.fillAll (Colour::greyLevel (0.9f));
}

void EditorComponent::resized()
{
  gainSlider->setBounds (10, 10, 150, 22);
  patchSlider->setBounds (170, 10, 150, 22);
  infoLabel->setBounds (10, 35, 450, 20);
  
  const int keyboardHeight = 70;
  midiKeyboard->setBounds (4, getHeight() - keyboardHeight - 4,
			   getWidth() - 8, keyboardHeight);
  
  resizer->setBounds (getWidth() - 16, getHeight() - 16, 16, 16);
  
  // if we've been resized, tell the SidEmu so that it can store the new size
  // in its settings
  getSidEmu()->lastUIWidth = getWidth();
  getSidEmu()->lastUIHeight = getHeight();
}

//==============================================================================
void EditorComponent::changeListenerCallback (void* source)
{
  // this is the SidEmu telling us that it's changed, so we'll update our
  // display of the time, midi message, etc.
  updateParametersFromSidEmu();
}

void EditorComponent::sliderValueChanged (Slider*)
{
  getSidEmu()->setParameterNotifyingHost (0, (float) gainSlider->getValue());
  getSidEmu()->setParameterNotifyingHost (2, (float) patchSlider->getValue());
}

//==============================================================================
void EditorComponent::updateParametersFromSidEmu()
{
  AudioProcessing* const sidEmu = getSidEmu();
  
  // we use this lock to make sure the processBlock() method isn't writing to the
  // lastMidiMessage variable while we're trying to read it, but be extra-careful to
  // only hold the lock for a minimum amount of time..
  sidEmu->getCallbackLock().enter();
  
  // take a local copy of the info we need while we've got the lock..
  const AudioPlayHead::CurrentPositionInfo positionInfo (sidEmu->lastPosInfo);
  const float newGain = sidEmu->getParameter(0);
  const float newBank = sidEmu->getParameter(1);
  const float newPatch = sidEmu->getParameter(2);
  
  // ..release the lock ASAP
  sidEmu->getCallbackLock().exit();
  
  
  // ..and after releasing the lock, we're free to do the time-consuming UI stuff..
  String infoText;
  infoText << String (positionInfo.bpm, 2) << T(" bpm, ")
  << positionInfo.timeSigNumerator << T("/") << positionInfo.timeSigDenominator
  << T("  -  ") << timeToTimecodeString (positionInfo.timeInSeconds)
  << T("  -  ") << ppqToBarsBeatsString (positionInfo.ppqPosition,
					 positionInfo.ppqPositionOfLastBarStart,
					 positionInfo.timeSigNumerator,
					 positionInfo.timeSigDenominator);
  
  if (positionInfo.isPlaying)
    infoText << T("  (playing)");
  
  infoLabel->setText (infoText, false);
  
  /* Update our slider.
   
   (note that it's important here to tell the slider not to send a change
   message, because that would cause it to call the sidEmu with a parameter
   change message again, and the values would drift out.
   */
  gainSlider->setValue(newGain, false);
  patchSlider->setValue(newPatch, false);
  
  setSize (sidEmu->lastUIWidth,
	   sidEmu->lastUIHeight);
}
