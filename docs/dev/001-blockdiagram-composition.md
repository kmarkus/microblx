# Extending the blocksdiagram DSL with Composition

**Authors**:

- Markus Klotzbuecher <mk@mkio.de>
- Enea Scioni <enea.scioni@kuleuven.be>

## Motivation

The current blockdiagram DSL does not support hierarchical
composition, which would be useful for building modular and reusable
compositions. This document outlines the requirements and sketches a
roadmap towards more complete composition support.

## Technical Requirements 

- The DSL must allow to include one or more subsystems
- An included subsystem *must not* require any knowledge of the
  supersystem (or that it is being included at all).
- It must be possible to override configurations of the subsystem from
  the super-system.
- It must be possible to connect to ports of an included subsystem
- It should be possible to remove elements of the subsystem from the
  supersystem (blocks, connections)
- The use of third party schedule computation tools via some form of
  plugin mechanism shall be supported.
- It must be possible to specify schedules manually. At the same time
  it should be possible to integrate third-party (e.g. via a plugin
  mechanism) scheduling tools.
- To support existing users, the extensions shall be as backward
  compatible as possible

## Approach

There are two relevant parts for supporting composition: the
structural inclusion of a subsystem (i.e. the blocks, configurations
and connections) and the run-time aspects (e.g. how to trigger such a
composition).

### Model extensions

A new keyword `subsystems` is included, which permits to include other
subsystems.

```
return bd.system {
	subsystems = {
		subsys1 = bd.load("subsys1.usc"),
		subsys2 = bd.load("subsys2.usc"),
	},
    [...]
```

Moreover, a `schedule` keyword is introduced, which permits to define
the triggering order of blocks:

```Lua
return bd.system {
	...
	schedule = { "block1", "block2", ... }
}
```

This can be used *instead* of just instantiating a `trig` or `ptrig`
and allows "late" selection of the activity that shall trigger the
composition.

Another use-case is to support multiple alternative schedules, that can be
selected at run-time.

### Structural aspects of composition

Composing one or more subsystems results in the following:

- the superset of all modules's `imports` will be loaded
- the blocks of all compositions are instantiated recursively
- configurations are applied to blocks (taking into account
  configurations that are overriden by higher composition layers.
- connections are established among blocks.

### Run-time aspects of composition

During instantiation of the system, the `schedule` of all compositions
can be used to configure passive `trig` blocks. These are points to
which active triggers such as `ptrig` can be attached during
startup. The `schedule` and the active trigger together form the
*activity model*.


### Example

**Basic robot composition** consisting of robot driver and
kinematics. This can be used as a basis for further applications.

```Lua
return bd.system {

	imports = { "stdtypes", ... },
	
	blocks = {
		{ name="robot_drv", type="robotX" },
		{ name="kin", type="kinematics" },
	},
	
	configurations = {
		{ name="kin", config = { model = "robot_model.urdf" } },
	},
	
	connections = {
		{ src="robot_drv.msr_jntpos", tgt="kin.msr_jntpos },
		{ src="robot_drv.msr_jntvel", tgt="kin.msr_jntvel },
		{ src="kin.cmd_jntvel, tgt="robot_drv.cmd_jntvel" },
	},
	
	schedule = {
	    { "robot_drv", "kin" },
	},
	
	[...]	
}
```

**Cartesian Trajectory**

This composition reuses the basic composition and just extends it with
a trajectory generator:

```Lua
return bd.system
	subsystems = {
		basic = bd.load("robot_basic.usc"),
	},
	
	blocks = {
		{ name="cart_tj", type="cartesian_trajgen" },
	},
	
	connections = {
		-- connecting to blocks of subcompositions
        { src="kin.msr_ee_pose", tgt="cart_tj.msr_pos" },
        { src="cart_tj.cmd_vel", tgt="kin.cmd_ee_twist" }, 
	},
	
	configurations = {
		-- override a sub-config
		{ name="basic.kin", config = { model = "robot_model2.urdf" } },
	},

	
	schedule = {
	    { "basic", "cart_tj" },
	},
```


## Extensibility

### Supporting custom schedule calculations

Instead of manually specified schedules, in many cases these can be
automatically calculated. To support custom or external tools, a
plugin mechanism shall be provided, which when invoked must populate
the `schedule` entries.

## Questions / Answers

### How to deal with block name clashes?

There needs to be a way to support dealing with block names clashes. A
simple approach would be to extend `load` to take the id as a
parameter, and then instantiate blocks with names `id.name`.

## Acknowledgement

- The composition approach, the activity model and component
  operational modes have been adopted from the RobMoSys Component
  Model.

- COCORF ITP project of H2020 RobMoSys


