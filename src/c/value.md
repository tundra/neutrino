Values
======

Neutrino's value layout and the functionality that services it is at the core of the runtime. For uniformity the model uses a number of terms like _value_, _species_, and _division_. This document describes the value layout and explains what all the terms mean.

## Values

The highest level of abstraction are _values_. A value is any piece of data used by the runtime, both those visible to the surface language like tagged integers and heap objects and those internal to the runtime like moved object forward pointers and conditions. A value is represented by `value_t` in the source code.

A value is a 64-bit unsigned integer value. The lowest 3 bits is a tag value used to distinguish the different flavors of value, the remaining 61 bits are the payload. For tagged integers the integer value is stored directly in the payload. For conditions it is the condition cause. For heap objects it is a pointer to the memory address where the object is located. These different types are called _domains_. Given a value you can ask for its domain and you'll get `vdTaggedInteger`, `vdHeapObject`, etc. back.

## Objects

Objects are a block of memory with a pointer to a descriptor, the object's _species_ (generally objects are described using words from biology).

Objects are divided into _families_, for instance `ofString`, `ofBool`, `ofArray`, etc. The family of a given object is stored in a field in the object's species. Each family has a _behavior_ struct which is just a struct of function pointers (like a v-table basically) that know how to operate on instances. The behavior is stored in the object's species. For instance, to get the heap size of an object you grab the object's species, grab the behavior struct, and call the `get_heap_object_layout` function passing in the object. You won't know what kind of object you were working with but you'll get back the heap size.

Within each family there may be objects with the same basic layout but different shared state, each kind of state is a _species_. For instance, all user-defined types are within the same family but each different type belong to a different species. The species is given by the object's species pointer.

Since species can hold state themselves, and different families need different kinds of shared state, species objects can themselves be structured in different ways. For instance: the species of an array holds no extra state, whereas the species of a user-defined type holds information about the type and the species of a string holds information about the string's encoding. These types are in different _divisions_, so strings are in one division, user-defined types in another, and arrays, bools, etc. are all in a third one.

In short: all values belong to some domain. Within the object domain objects are divided into divisions based on their species' layout, each division is again divided into families based on instances' layout, and finally some families are divided into species based on data that is shared between all objects of the same species.

## Object layout

Heap objects all have a common layout:

 * The first word is a pointer to the object's _header_. During normal execution the header is a _species_ object, during garbage collection it may be a moved object pointer.
 * After the header comes any "dumb" data, that is, data that doesn't have to be updated when the object is moved by the garbage collector.
 * After the dumb data comes values that must be updated during GC.

This simplifies handling of objects. You don't have to know the exact type of an object to move it, for instance, you just need to know its size and the offset of the value section and it can be moved correctly.

## Modes

Any value can be in one of four modes. These modes indicate which operations are legal. A value starts out least restricted and can then move towards more restrictions, never fewer. The modes are:

  * **Fluid** Any changes can be made to this object, including changing which protocol it supports and which fields it has.
  * **Mutable** The object's fields can be set but no changes can be made to which fields exist or which primary protocol the value has.
  * **Frozen** The object cannot be changed but can reference other objects that can.
  * **Deep frozen** The object cannot change and neither can any objects it references.

Not all values can be in all states. For instance, integers and strings start out deep frozen so the less restricted states don't apply to them.

You can explicitly move an object to the mutable or frozen mode if you know the object is in a less restricted mode but you can't make it deep frozen, and you don't need to. Being deep frozen is a property of a full object graph so you can ask if an object is deep frozen and the object graph will be traversed for you to determine whether it is. That traversal may cause the object to be marked as deep frozen.

## Ownership

Some values are considered to logically *own* other values. For instance, an id hash map owns its entry array, an array buffer owns it storage array, and the roots object owns all the roots. Basically, if creating one object requires another object to be created that isn't passed to it from elsewhere then the object is owned. The place you see this is mainly around freezing. If you freeze an object (not shallow-freeze, freeze) then the object is responsible for freezing any objects it owns. You can think of it as a matter of encapsulation. If an object needs some other objects to function which it otherwise doesn't expose to you then that's that object's business and it should be transparent when freezing it that those other objects exist. If freezing it didn't recursively freeze those you could tell they existed because they would prevent it from being deep frozen.

Ownership is strictly linear: if object *a* owns object *b* then *b* may itself own other objects, but it must not be the case that *b* or something transitively owned by *b* considers itself to own *a*.
