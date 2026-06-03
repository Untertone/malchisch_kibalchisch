# What's in the Scene (CS)


The VR Template comes with a demo scene where you can grab, hold, throw, and use various objects. Start the application and you'll find a table with several primitive shapes, a laser pointer, and a button on its surface.


What each object can do is defined by the components attached to it. For example, geometric primitives have the [VRPhysicMovableObject](../../...md) component (grab, hold, throw), while the laser pointer also has [VRLaserPointer](../../...md) (emits a beam when activated).


## What's in the Scene


Here is what you can interact with in the demo scene:


### Movable Objects


Pick up any of the geometric primitives on the table -hold them, toss them around, watch them bounce off walls. When you let go, the object keeps the momentum of your hand.


![](movable_objects.png)


Built with: [VRPhysicMovableObject](../../...md), [VRKinematicMovableObject](../../...md), [VRTransformMovableObject](../../...md)


### Switches


Grab the button on the table to toggle it -it animates between two positions with a click.


![](switches.png)


Built with: [VRObjectSwitch](../../...md), [VRNodeSwitchEnableByGrab](../../...md)


### Handles


Grab a lever or a valve and move it -handles constrain your motion to a specific axis or range, so they feel like real mechanical controls.


![](handles.png)


Built with: [VRObjectHandleRotatable](../../...md), [VRObjectHandleTranslatable](../../...md), [VRObjectHandle](../../...md)


### Laser Pointer


Pick up the laser pointer and activate it -it casts a ray and shows the name of whatever you are pointing at.


![](laser_pointer.png)


Built with: [VRTransformMovableObject](../../...md), [VRLaserPointer](../../...md)


### Gun


Grab the gun and pull the trigger to fire. It shoots a ray from the muzzle with sound and visual effects, knocking physics objects on impact.


![](gun.png)


Built with: [VRGun](../../...md)


### Cables, Plugs & Sockets


Pick up a cable end and bring it close to a socket -it snaps in automatically. In the demo, connecting both ends of a cable to the generator powers on a lamp.


![](cables.png)


Built with: [VRObjectPhysicalCable](../../...md), [VRPluggable](../../...md), [VRSocketObject](../../...md)


### Inventory


Open the inventory and throw an object toward the grid -it snaps into a free cell. Grab it back to take it out.


![](inventory.png)


Built with: [VRInventory](../../...md), [VRInventoryItem](../../...md)


### Platforms


Step onto a moving platform -it carries you along, automatically compensating for its velocity so you stay stable.


Built with: [VRObjectPlatform](../../...md)


For a complete list of all classes and components, see the [Classes & Components Reference](../../../vr_development/vr_template/vr_template_classes_and_components/index_cs.md).
