# Eye Tracking (CPP)


If your headset can track where you're looking, the VR Template puts that to use - look at an object and the system identifies it and shows its name as a floating label.


> **Notice:** Eye tracking requires a compatible headset and is available through the Varjo and OpenXR backends.


## How It Works


Two properties work together to make this happen:


- **[EyetrackingPointer](../../...md)** - takes the gaze direction from the eye tracking system and casts a ray into the world. If it hits something, that's what you're looking at.
- **[ObjectLabeling](../../...md)** - takes the hit result and displays the object's name as a floating label at the intersection point. The label follows your gaze and updates every frame.


The gaze ray extends up to 10,000 units and only considers physical objects - GUI elements are filtered out. If you're not looking at anything labeled, the text hides automatically.


To set it up, assign an *ObjectText* node to the *text* field of *[ObjectLabeling](../../...md)* - the system will position and update it based on gaze data.


Built with: [EyetrackingPointer](../../...md), [ObjectLabeling](../../...md)
