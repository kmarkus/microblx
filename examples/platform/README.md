Closed-loop control of a simulated 2-DoF platform:

- `platform_2dof.lua`, `platform_2dof_control.lua`: block models of
  the plant and the controller
- `platform_2dof.c`, `platform_2dof_control.c`: hook implementations
- `platform_launch/platform_2dof_and_control.usc`: the composition
- `platform_launch/main.c`: the same system launched from C

```sh
ubx-genblock -c platform_2dof.lua -d platform_2dof
cp platform_2dof.c platform_2dof/
cd platform_2dof && ./bootstrap && ./configure && make && sudo make install
# same for platform_2dof_control, then
ubx-launch -c platform_launch/platform_2dof_and_control.usc
ubx-mq read plat1.pos
```
