# Extending the blockdiagram DSL with composition support

**Authors**:

- Markus Klotzbuecher <mk@mkio.de>
- Enea Scioni <enea.scioni@kuleuven.be>

## Motivation

The current microblx blockdiagram DSL does not support hierarchical
composition, which is required for building modular and reusable
compositions. This document outlines the requirements and outlines a
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
  supersystem (blocks, connections).
- The use of (third party) schedule computation tools via a form of
  plugin mechanism shall be supported. Nevertheless, it must be
  possible to specify schedules manually.
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
in the scope of this concept.

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


### Structural aspects of composition

Structurally, composing one or more subsystems by including these
results in the following:

- the superset of all modules's `imports` will be loaded
- the blocks of all compositions are instantiated recursively
- configurations are applied to blocks (taking into account
  configurations that are overriden by higher composition layers.
- connections are established among blocks.

### Run-time aspects of composition

For a composable usc model it is imperative that it contains no active
triggers (such as the pthread `ptrig` trigger), as introducing these
must exclusively be in the responsibility of the system builder.

Thus, the manual way to achieve this is by adding a passive schedule
block `trig` to each composition. This block encapsulates the schedule
of it's blocks and subsystems and provides a triggering entry point
for the parent composition, while avoiding platform specific
activities.

To bring in the latter, a separate *activity model* is introduced
similar to the RobMoSys Activity Architecture DSL [1]. This model is
*not* part of the `system` specification, but is *bound* to a given
composition at a late stage, most likely during launching of a
composition:

```sh
$ ubx_launch -c mycomp.usc -a '{ name="p1" type="ptrig", tgt="trig1", config={ sched_priority=99, period = { ... } }'
```

Apart from instantatiating the composition described in `mycomp.usc`,
this instantiates an active trigger `ptrig1`, configures it with the
given configuration (priorities, etc.) and attaches it to the passive
trigger `trig1`, which encapsulates the top-level schedule for the
composition.

By using these passive `trig` blocks, a triggering `hierarchy` is
formed, which can be controlled by the system builder. Moreover, this
approach can easily be replaced by automatic schedule calculation.

#### Update 2020-01-23

Reconsidering, the above `-a` option seems to limited. What it
basically permits is to compose two systems at a late stage. So a more
generic mechanism would be simply to alloc `-c` to accept multiple
systems and to merge these into one before launching.

### Example

The following is a **basic robot composition** consisting of robot
driver and kinematics. The idea is to reuse this composition as a
basis for further applications.

```Lua
return bd.system {

	imports = { "stdtypes", ... },
	
	blocks = {
		{ name="robot_drv", type="robotX" },
		{ name="kin", type="kinematics" },
		{ name="trig", type="std_triggers/trig" },
	},
	
	configurations = {
		{ name="robot_drv", config = { inteface="can0" } },
		{ name="kin", config = { model = "robot_model.urdf" } },
		{ name="trig", config = { trig_blocks =
			                        { b="#robot_drv", num_steps=1 },
									{ b="#kin",       num_steps=1 } } },
	},
	
	connections = {
		{ src="robot_drv.msr_jntpos", tgt="kin.msr_jntpos" },
		{ src="robot_drv.msr_jntvel", tgt="kin.msr_jntvel" },
		{ src="kin.cmd_jntvel", tgt="robot_drv.cmd_jntvel" },
	},
	
	[...]	
}
```

**Cartesian Trajectory**

This composition reuses the basic composition and just extends it with
a trajectory generator:

```Lua
return bd.system {
	subsystems = {
		basic = bd.load("robot_basic.usc"),
	},
	
	blocks = {
		{ name="cart_tj", type="cartesian_trajgen" },
		{ name="trig", type="std_triggers/trig" },
	},
	
	connections = {
		-- connecting to blocks of subcompositions
        { src="kin.msr_ee_pose", tgt="cart_tj.msr_pos" },
        { src="cart_tj.cmd_vel", tgt="kin.cmd_ee_twist" }, 
	},
	
	configurations = {
		-- override a sub-config
		{ name="basic.kin", config = { model = "robot_model2.urdf" } }
		{ name="trig", config = { trig_blocks =
			                        { b="#cart_tj",    num_steps=1 },
									{ b="#basic.trig", num_steps=1 } } },
	},
},
	
```

## Extensibility

The above example illustrates, how the basic mechanisms can be used to
specify composable usc files. However, instead of manually specifying
schedules, in many cases these can be automatically calculated
(e.g. by deriving them from the data-flow). This use-case will be
supported by a plugin mechanism, which can (among other use-cases) be
used to automatically compute the schedule for a trig block.

## Questions / Answers

### How to deal with block name clashes?

There needs to be a way to support dealing with block names clashes. A
simple approach would be to extend `load` to take the id as a
parameter, and then instantiate blocks with names `id.name`.

## Acknowledgment

- Several concepts such as the composition approach and the activity
  model have been adopted from the RobMoSys Modeling approach [1],
  [2].

- The COCORF ITP project of H2020 RobMoSys

# References

[1] https://robmosys.eu/wiki/modeling:metamodels:system  
[2] https://robmosys.eu/wiki/modeling:metamodels:deployment  


