#include <vector>

#include <kdl/velocityprofile_trap.hpp>
#include <kdl/kdl.hpp>
#include <kdl/frames.hpp>

#include "cart_trajgen.hpp"

using namespace std;

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
	
		/* record starting time */
		ubx_clock_mono_gettime(&inf->start_time);

		/* compute twist to reach target_pos */
		diff_vel = KDL::diff(inf->start_pos, inf->target_pos);
		
		for (int i=0; i<6; i++) {
			inf->motion_profile[0][i].SetProfileDuration(0, diff_vel(i), des_dur);
			inf->max_duration = inf->motion_profile[0][i].Duration();
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
	ubx_ts_sub(&inf->start_time, &now, &diff);
	time_passed = ubx_ts_to_double(&diff);
	write_move_dur(inf->ports.move_dur, &time_passed);

	/*
	 * send out trajectory
	 */
	if (inf->is_moving==1) {
		if(time_passed > inf->max_duration) {
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

		write_cmd_pos(inf->ports.cmd_pos, (struct kdl_frame*) &cmd_pos);
		write_cmd_vel(inf->ports.cmd_vel, (struct kdl_twist*) &cmd_twist);
	}
}

