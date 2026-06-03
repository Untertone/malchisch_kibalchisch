# Classes and Components Overview (CS)


When you first open the VR Template project and see a lot of components, it can feel a bit overwhelming. Let's walk through the main parts so you know where everything lives. All VR components are assigned to nodes organized under the ***vr_layer*** root node:


![VR Template Structure (C#)](../../../learn/13_vr_app/vr_layer_cs_structure_with_captions.png)


## VRPlayer Component


The player is represented by a node hierarchy rooted at ***vr_player*** (a *Player Dummy* camera) with the ***[VRPlayer](../../...md)*** component assigned. Child nodes represent HMD and controller models, each with their own components for interactions and settings.


## InputSystem Component


***[InputSystem](../../...md)*** is the central hub for all input. It manages available devices, input sources, and event handlers, supporting both VR and PC modes with automatic switching.


The actual input logic lives in specialized classes like ***[VRInput](../../...md)*** and ***[PCInput](../../...md)***, each handling head position, hand controllers, gestures, buttons, and actions like jump or crouch.


- **General Input Components**

  - ***[InputSystem](../../...md)*** - coordinates input across all platforms.
  - ***[VRInput](../../...md)*** - VR-specific input: headset pose, controller tracking, button states.
- **VR Input Components** (active when *vr_input* is enabled)

  - ***[VRGeneralInput](../../...md)*** - general VR actions (movement, jump, fire, crouch) bound to controller buttons/axes.
  - ***[VRControllerInput](../../...md)*** - per-hand controller input: button presses, touches, axis values.
  - ***[VRHandTrackingInput](../../...md)*** - hand tracking input with gesture recognition and per-finger tracking.
- **PC Input Components** (active when *pc_input* is enabled)

  - ***[PCInput](../../...md)*** - keyboard and mouse input.
  - ***[PCGeneralInput](../../...md)*** - maps keyboard/mouse to generalized actions for desktop mode.


## VRBaseController Component


Input devices are represented by components inherited from ***[HeadController](../../...md)*** (for HMDs) and ***[HandController](../../...md)*** (for controllers). They handle tracking and positioning, activating automatically based on the current input system.


- ***[VRBaseController](../../...md)*** - base class for all control components.

  - ***[HeadController](../../...md)*** - updates head position.

    - ***[VRHeadController](../../...md)*** - uses VR headset tracking data.
    - ***[PCHeadController](../../...md)*** - desktop mode.
  - ***[HandController](../../...md)*** - updates hand position.

    - ***[VRHandController](../../...md)*** - VR controller position and visuals.
    - ***[PCHandController](../../...md)*** - simulated hand for desktop input.
    - ***[VRHandTracking](../../...md)*** - hand bones and animations from tracking data.
  - ***[BasestationController](../../...md)*** - updates VR base station positions and displays their 3D models.

    - ***[VRBasestationController](../../...md)*** - VR-specific base station tracking.


## VRMovementManager Component


***[VRMovementManager](../../...md)*** runs all active movement components each frame. Specific behaviors are built as separate components inheriting from ***[VRBaseMovement](../../...md)***:


- ***[WalkMovement](../../...md)*** and ***[PCWalkMovement](../../...md)*** - moving through the world using controller sticks or keyboard.
- ***[TurnMovement](../../...md)*** and ***[PCTurnMovement](../../...md)*** - rotating using buttons or mouse.
- ***[TeleportationMovement](../../...md)*** - instant relocation via teleport ray.
- ***[VRHandTrackingTeleportMovement](../../...md)*** - teleportation via hand gestures.
- ***[CrouchMovement](../../...md)*** - crouching on button press.


All movement components are modular - mix and match them in the Editor to build the locomotion scheme you need.


## VRInteractionManager Component


***[VRInteractionManager](../../...md)*** manages interactions between the player and scene objects - tracking hover, grab, and use states. Each interaction type is a separate component derived from ***[VRBaseInteraction](../../...md)***:


- ***[VRHandMenuInteraction](../../...md)*** - UI interaction via controller pointing ray.
- ***[VRHandTrackingMenuInteraction](../../...md)*** - UI interaction via hand tracking gestures.
- ***[VRHandShapeInteraction](../../...md)*** - grabbing, using, and hovering over physical objects via a trigger shape.
- ***[VRPCHeadMenuInteraction](../../...md)*** - menu interaction in desktop mode using mouse.


They're enabled automatically depending on device availability.


## VRBaseInteractable Component


***[VRBaseInteractable](../../...md)*** is the base for all interactive objects - it defines what a player can do with them. You can also [create your own interactable types](../../../vr_development/vr_template/vr_template_new_interactable/index_cs.md).


The following components inherit from ***VRBaseInteractable***:


| [VRTransformMovableObject](../../...md) | Objects you can grab, hold, and throw - transform-based movement (no physics). |
|---|---|
| [VRKinematicMovableObject](../../...md) | Objects you can grab, hold, and throw - kinematic physics. |
| [VRPhysicMovableObject](../../...md) | Objects with rigid bodies that you can grab, hold, and throw - full physics simulation. |
| [VRObjectHandle](../../...md) | Handles, levers, valves - things you can turn or move while holding. |
| [VRObjectHandleTranslatable](../../...md) | Handles that slide along a limited linear path. Supports toggle animation and sound feedback. |
| [VRObjectHandleRotatable](../../...md) | Handles that rotate around a specified axis. |
| [VRObjectSwitch](../../...md) | Buttons and switches - toggled by grabbing. Works with ***VRNodeSwitchEnableByGrab*** for additional effects. |
| [VRNodeSwitchEnableByGrab](../../...md) | Toggles one or more nodes on/off when grabbed. |
| [VRNodeSwitchEnableByKey](../../...md) | Toggles nodes by pressing a button while holding the object. Works in both VR and PC modes. |
| [VRLaserPointer](../../...md) | Casts a laser ray. The laser pointer node has both ***VRTransformMovableObject*** and ***VRLaserPointer*** assigned. |
| [VRGun](../../...md) | A firearm you can grab, hold, and shoot - with sound/visual effects and **VRGunSlide** for slide animation. |
| [VRPluggable](../../...md) | An object that auto-attaches to a matching **VRSocketObject** when released nearby. |
| [VRSelectionTest](../../...md) | Shows a visual outline on hover/grab - useful for debugging interaction feedback. |


Additional supporting components:


| [VRObjectPlatform](../../...md) | A movable platform that carries the player when they stand on it. |
|---|---|
| [VRGunSlide](../../...md) | Animates a firearm's slide mechanism. |
| [VRSocketObject](../../...md) | Defines a socket connection point for pluggable objects. |
| [VRObjectPhysicalCable](../../...md) | A physics-simulated cable connecting two pluggable ends. |
| [Generator](../../...md) | Activates when both sockets are connected. Broadcasts a signal to receivers. |


## Inventory System


A spatial, grid-based storage that appears in front of you. Throw objects in, grab them out.


| [VRInventory](../../...md) | The grid itself - handles item placement, removal, scaling, and highlight animations. |
|---|---|
| [VRInventoryItem](../../...md) | Defines how an item is positioned when placed in the grid. |


## GUI Classes


In-world menu components, all inheriting from ***[VRBaseUI](../../...md)***:


| [VRMenuSample](../../...md) | A sample menu showing basic widget creation and button interaction. |
|---|---|
| [VRMenuSettings](../../...md) | A settings menu for toggling between smooth and step rotation. |


## Object Attachment


Need something to follow the player - a menu on your wrist, a HUD in front of your eyes?


| [VRAttachToHand](../../...md) | Locks a node to the player's hand. Choose left or right, set the offset. |
|---|---|
| [VRAttachToHead](../../...md) | Locks a node to the headset so it follows your gaze. |


## Signal System


A one-to-many event system for connecting powered devices:


| [SignalEmitter](../../...md) | Broadcasts float, int, and string values to connected listeners. |
|---|---|
| **ISignalSlot** | An interface for event receivers. Implement *ReceiveFloat*, *ReceiveInt*, and *ReceiveString* callbacks to respond to signals (see **Lamp**, **DoorTranslatable**). |
| [SignalLinkOneToMany](../../...md) | Connects one emitter to multiple slots. |


The following components react to signals (implement **ISignalSlot**):


| [Lamp](../../...md) | Toggles a light on or off in response to an incoming signal. |
|---|---|
| [DoorTranslatable](../../...md) | Slides a door between open and closed positions in response to a signal. |


Example: a **Generator** emits a signal -> **SignalLinkOneToMany** forwards it -> a **Lamp** lights up and a **DoorTranslatable** slides open.


## Visual Feedback


Visual feedback during interaction:


| [ObjectOutliner](../../...md) | Highlights objects with an outline when your hand gets close. |
|---|---|
| [VRSocketOutliner](../../...md) | Highlights sockets when a compatible plug is nearby. |
| [VRTooltip](../../...md) | Floating text labels near objects during interaction. |
| [TooltipTextSwitcher](../../...md) | Switches tooltip text based on the current input mode. |


## Framework


The framework layer includes the [Component System](../../../principles/component_system/index.md) and utility classes.


### PlayerMotor


**[PlayerMotor](../../...md)** - physics-based locomotion: movement, collisions, crouching, jumping, stepping over obstacles, slopes, and moving platforms.


### Triggers


**[Triggers](../../...md)** - marks room obstacles and warns the player with haptic feedback as they get closer.


### HeadCollision


**[HeadCollision](../../...md)** - detects real-room obstacles near the player's head with visual and haptic warnings.


### MaskHolder


**[MaskHolder](../../...md)** - centralized access to physics masks for teleportation, interaction, and grab detection.


### SoundManager


**[SoundManager](../../...md)** - audio playback: one-shot effects, looped sounds, grouping, and volume control.


### Lifetime


**[Lifetime](../../...md)** - auto-deletes its node after a set duration.


### TrajectoryMovement


**[TrajectoryMovement](../../...md)** - moves objects along a parabolic arc for teleport visualization.


### Utils


Helper functions for 3D math, transforms, geometry, visualization, and world management.
