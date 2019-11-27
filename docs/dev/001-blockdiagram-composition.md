# Extending the blockdiagram DSL with Composition

**Authors**:

- Markus Klotzbuecher <mk@mkio.de>
- Enea Scioni <enea.scioni@kuleuven.be>

## Motivation

The current blockdiagram DSL does not support hierarchical
composition, which would be useful for building modular and reusable
compositions. This document outlines the requirements and sketches a
roadmap towards more complete composition support.

For background about this, see the documentation section `Composing
microblx systems`.

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

This concept is focused on outlining the basic mechanisms to allow
composition for the function block composition DSL. Advanced concepts
as automatic schedule computation are foreseen in the design, but not
in the scope of this effort.

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

For a composable usc model it is imperative that it contains no active
triggers (e.g. `ptrig`), as introducing these must be in the
responsibility of the system builder.

The basic way to achieve this is by adding a passive schedule block
`trig` to each composition. This provides a triggering entry point for
the parent composition, but at the same time avoids assumptions about
activities.

To bring in the latter, a separate *activity model* is
introduced. This is *not* part of the ``bd.system`` specification, but
is *bound* to a given composition at a late stage, e.g. during
launching a composition:

```sh
$ ubx_launch -c mycomp.usc -a '{ name="p1" type="ptrig", tgt="trig1", config={ sched_priority=99, period = { ... } }'
```

This instantiates an active trigger `ptrig1`, configures it with the
given configuration and attaches it to the passive trigger `trig1`,
which contains the top-level schedule for the composition.

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
		{ name="trig", type="std_triggers/trig" },
	},
	
	configurations = {
		{ name="kin", config = { model = "robot_model.urdf" } },
		{ name="trig", config = {
	},
	
	connections = {
		{ src="robot_drv.msr_jntpos", tgt="kin.msr_jntpos },
		{ src="robot_drv.msr_jntvel", tgt="kin.msr_jntvel },
		{ src="kin.cmd_jntvel, tgt="robot_drv.cmd_jntvel" },
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

## Acknowledgment

- The composition approach, the activity model and component
  operational modes have been adopted from the RobMoSys Component
  Model.

- COCORF ITP project of H2020 RobMoSys

# References

[1] Activity Model


