// Cartesian Trajectory Generator
//
// Copyright (C) 2003 Klaas Gadeyne <klaas.gadeyne@mech.kuleuven.ac.be>
//                    Wim Meeussen  <wim.meeussen@mech.kuleuven.ac.be>
// Copyright (C) 2006 Ruben Smits <ruben.smits@mech.kuleuven.ac.be>
// Copyright (C) 2014 Markus Klotzbuecher <markus.klotzbuecher@mech.kuleuven.be>
//

#include <vector>

#include <kdl/velocityprofile_trap.hpp>
#include <kdl/kdl.hpp>
#include <kdl/frames.hpp>
#include "../std_types/kdl/types/kdl.h"

#include "cart_trajgen.hpp"

UBX_MODULE_LICENSE_SPDX(GPL-2.0+)

using namespace std;

void print_kdl_frame(struct kdl_frame* f)
{
#ifdef DEBUG
	printf("%3f %3f %3f %3f\n%3f %3f %3f %3f\n%3f %3f %3f %3f\n%3f %3f %3f %3f\n",
		       f->M.data[0], f->M.data[1], f->M.data[2], f->p.x,
		       f->M.data[3], f->M.data[4], f->M.data[5], f->p.y,
		       f->M.data[6], f->M.data[7], f->M.data[8], f->p.z,
		       0.0, 0.0, 0.0, 1.0);
#endif
}

void print_kdl_twist(struct kdl_twist* t)
{
#ifdef DEBUG
	printf("      %3f\n", t->vel.x);
	printf("vel = %3f\n", t->vel.y);
	printf("      %3f\n", t->vel.z);
	printf("      %3f\n", t->rot.x);
	printf("rot = %3f\n", t->rot.y);
	printf("      %3f\n", t->rot.z);
#endif
}

/* define a structure for holding the block local state. By assigning an
 * instance of this struct to the block private_data pointer (see init), this
 * information becomes accessible within the hook functions.
 */
struct cart_trajgen_info
{
	int is_moving;
	double max_duration;
	KDL::Frame target_pos, start_pos;

	vector<KDL::VelocityProfile_Trap> *motion_profile;

	struct ubx_timespec start_time;

	/* this is to have fast access to ports for reading and writing, without
	 * needing a hash table lookup */
	struct cart_trajgen_port_cache ports;
};

/* init */
int cart_trajgen_init(ubx_block_t *b)
{
	int ret = -1;
	struct cart_trajgen_info *inf;

	/* allocate memory for the block local state */
	if ((inf = (struct cart_trajgen_info*)calloc(1, sizeof(struct cart_trajgen_info)))==NULL) {
		ERR("cart_trajgen: failed to alloc memory");
		ret=EOUTOFMEM;
		goto out;
	}
	b->private_data=inf;

	inf->motion_profile = new
		vector<KDL::VelocityProfile_Trap>(6, KDL::VelocityProfile_Trap(0, 0));


	update_port_cache(b, &inf->ports);
	ret=0;
out:
	return ret;
}

/* start */
int cart_trajgen_start(ubx_block_t *b)
{
	struct cart_trajgen_info *inf = (struct cart_trajgen_info*) b->private_data;
	int ret = 0;
	unsigned int len;

	KDL::Twist *max_vel, *max_acc;

	max_vel = (KDL::Twist*) ubx_config_get_data_ptr(b, "max_vel", &len);
	max_acc = (KDL::Twist*) ubx_config_get_data_ptr(b, "max_acc", &len);

	for (int i=0; i<3; i++) {
		inf->motion_profile[0][i].SetMax(max_vel->vel[i], max_acc->vel[i]);
		inf->motion_profile[0][i+3].SetMax(max_vel->rot[i], max_acc->rot[i]);
	}

	inf->is_moving = 0;
	return ret;
}

/* stop */
void cart_trajgen_stop(ubx_block_t *b)
{
	// struct cart_trajgen_info *inf = (struct cart_trajgen_info*) b->private_data;
}

/* cleanup */
void cart_trajgen_cleanup(ubx_block_t *b)
{
	struct cart_trajgen_info *inf = (struct cart_trajgen_info*) b->private_data;
	delete inf->motion_profile;
	free(b->private_data);
}

/* step */
void cart_trajgen_step(ubx_block_t *b)
{
	int tmp;
	double des_dur, time_passed;
	KDL::Twist cmd_twist, diff_vel;
	KDL::Frame cmd_pos;
	struct ubx_timespec now, diff;

	struct cart_trajgen_info *inf = (struct cart_trajgen_info*) b->private_data;

	/*
	 * new move_to command
	 */
	if (inf->is_moving==0 &&
	    read_des_pos(inf->ports.des_pos, ((struct kdl_frame*) &inf->target_pos)) == 1 &&
	    read_msr_pos(inf->ports.msr_pos, ((struct kdl_frame*) &inf->start_pos)) == 1 &&
	    read_des_dur(inf->ports.des_dur, &des_dur) == 1) {

#ifdef DEBUG
		printf("starting new trajectory\n");
		printf("des_dur %f", des_dur);
		printf("start pos:\n");
		print_kdl_frame((struct kdl_frame*) &inf->start_pos);
		printf("end pos:\n");
		print_kdl_frame((struct kdl_frame*) &inf->target_pos);
#endif

		/* record starting time */
		ubx_clock_mono_gettime(&inf->start_time);

		/* compute twist to reach target_pos */
		diff_vel = KDL::diff(inf->start_pos, inf->target_pos);

		for (int i=0; i<6; i++) {
			inf->motion_profile[0][i].SetProfileDuration(0, diff_vel(i), des_dur);
			inf->max_duration = MAX(0, inf->motion_profile[0][i].Duration());
		}

		/* Rescale trajectories to maximal duration */
		for (int i=0; i<6; i++)
			inf->motion_profile[0][i].SetProfileDuration(0, diff_vel(i), inf->max_duration);

		inf->is_moving=1;
		tmp=0;
		write_reached(inf->ports.reached, &tmp);
	}

	/* update time */
	ubx_clock_mono_gettime(&now);
	ubx_ts_sub(&now, &inf->start_time, &diff);
	time_passed = ubx_ts_to_double(&diff);
	write_move_dur(inf->ports.move_dur, &time_passed);

	/*
	 * send out trajectory
	 */
	if (inf->is_moving==1) {
		DBG("is_moving == 1, time_passed=%f, max_dur=%f", time_passed, inf->max_duration);
		if(time_passed > inf->max_duration) {
			DBG("time out: trajectory completed");
			inf->is_moving=0;
			cmd_pos = inf->target_pos;
			KDL::SetToZero(cmd_twist);
			tmp=1;
			write_reached(inf->ports.reached, &tmp);
		} else {
			/* position */
			diff_vel = KDL::Twist( KDL::Vector( inf->motion_profile[0][0].Pos(time_passed),
							    inf->motion_profile[0][1].Pos(time_passed),
							    inf->motion_profile[0][2].Pos(time_passed)),
					       KDL::Vector( inf->motion_profile[0][3].Pos(time_passed),
							    inf->motion_profile[0][4].Pos(time_passed),
							    inf->motion_profile[0][5].Pos(time_passed)));

			cmd_pos = KDL::Frame( inf->start_pos.M * Rot( inf->start_pos.M.Inverse(diff_vel.rot)),
					      inf->start_pos.p + diff_vel.vel);

			/* velocity */
			for (int i=0; i<6; i++)
				cmd_twist(i) = inf->motion_profile[0][i].Vel(time_passed);
		}

		DBG("outputting cmd_pos");
		print_kdl_frame((struct kdl_frame*) &cmd_pos);
		DBG("outputting cmd_vel");
		print_kdl_twist((struct kdl_twist*) &cmd_twist);

		write_cmd_pos(inf->ports.cmd_pos, (struct kdl_frame*) &cmd_pos);
		write_cmd_vel(inf->ports.cmd_vel, (struct kdl_twist*) &cmd_twist);
	}
}
