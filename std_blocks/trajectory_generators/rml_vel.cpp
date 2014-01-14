#include "rml_vel.hpp"

#include <ReflexxesAPI.h>
#include <RMLPositionFlags.h>
#include <RMLPositionInputParameters.h>
#include <RMLPositionOutputParameters.h>

/* define a structure for holding the block local state. By assigning an
 * instance of this struct to the block private_data pointer (see init), this
 * information becomes accessible within the hook functions.
 */
struct rml_vel_info
{
	ReflexxesAPI *RML;
	RMLPositionInputParameters *IP;
	RMLPositionOutputParameters *OP;
	RMLPositionFlags Flags;

	/* this is to have fast access to ports for reading and writing, without
	 * needing a hash table lookup */
	struct rml_vel_port_cache ports;
};


/* init */
int rml_vel_init(ubx_block_t *b)
{
	int ret = -1;
	unsigned int len;
	struct rml_vel_info *inf;
	double cycle_time;

	/* allocate memory for the block local state */
	if ((inf = (rml_vel_info*) calloc(1, sizeof(struct rml_vel_info)))==NULL) {
		ERR("rml_vel: failed to alloc memory");
		ret=EOUTOFMEM;
		goto out;
	}

	b->private_data=inf;

	cycle_time = *((double*) ubx_config_get_data_ptr(b, "cycle_time", &len));

	if(cycle_time <= 0) {
		ERR("invalid config cycle_time %lf", cycle_time);
		goto out_free;
	};

	inf->RML = new ReflexxesAPI( 5, cycle_time );
	inf->IP = new RMLPositionInputParameters( 5 );
	inf->OP = new RMLPositionOutputParameters( 5 );

	update_port_cache(b, &inf->ports);
	ret=0;
	goto out;

 out_free:
	free(b->private_data);
	b->private_data=NULL;
 out:
	return ret;
}

/* start */
int rml_vel_start(ubx_block_t *b)
{
	int ret = -1;
	unsigned int len;
	double *max_vel;

	struct rml_vel_info *inf = (struct rml_vel_info*) b->private_data;

	max_vel = (double*)ubx_config_get_data_ptr(b, "max_vel", &len);

	if(len != 5) {
		ERR("invalid array dimensions of config max_vel. is %d but should be %d", len, 5);
		goto out;
	}

	memcpy(inf->IP->MaxVelocityVector->VecData, max_vel, 5);
	ret = 0;
 out:
	return ret;
}

/* stop */
void rml_vel_stop(ubx_block_t *b)
{
	/* struct _info *inf = (struct _info*) b->private_data; */
}

/* cleanup */
void rml_vel_cleanup(ubx_block_t *b)
{
	struct rml_vel_info *inf;
	inf = (struct rml_vel_info*) b->private_data;

	delete inf->RML;
	delete inf->IP;
	delete inf->OP;

	free(b->private_data);
}

/* step */
void rml_vel_step(ubx_block_t *b)
{
	int i, res;
	double msr_pos[5], msr_vel[5];
	double des_pos[5], des_vel[5];
	double cmd_pos[5], cmd_vel[5], cmd_acc[5];
	int sz_msr_pos, sz_msr_vel, sz_des_pos, sz_des_vel;

	struct rml_vel_info *inf = (struct rml_vel_info*) b->private_data;

	/* new target pos? */
	sz_des_pos = read_des_pos_5(inf->ports.des_pos, &des_pos);

	if(sz_des_pos == 5)
		inf->IP->TargetPositionVector->VecData=des_pos;

	/* new target vel? */
	sz_des_vel = read_des_vel_5(inf->ports.des_vel, &des_vel);

	if(sz_des_vel == 5)
		inf->IP->TargetVelocityVector->VecData=des_vel;

	/* new measured pos? */
	sz_msr_pos = read_msr_pos_5(inf->ports.msr_pos, &msr_pos);

	if(sz_msr_pos == 5)
		inf->IP->CurrentPositionVector->VecData = msr_pos;

	/* new measured vel? */
	sz_msr_vel = read_msr_vel_5(inf->ports.msr_vel, &msr_vel);

	if(sz_msr_vel == 5)
		inf->IP->CurrentVelocityVector->VecData = msr_vel;

	/* select all DOF's */
	for(i=0; i<5; i++)
		inf->IP->SelectionVector->VecData[i] = true;


	/* update */
	res = inf->RML->RMLPosition(*inf->IP, inf->OP, inf->Flags);

	switch (res) {
	case ReflexxesAPI::RML_WORKING:
		break;
	case ReflexxesAPI::RML_FINAL_STATE_REACHED:
		i=1; /* reuse i to emit "reached" event */
		write_reached(inf->ports.reached, &i);
		break;
	case ReflexxesAPI::RML_ERROR_INVALID_INPUT_VALUES:
		ERR("RML_ERROR_INVALID_INPUT_VALUES");
		goto out_err;
	case ReflexxesAPI::RML_ERROR_EXECUTION_TIME_CALCULATION:
		ERR("RML_ERROR_EXECUTION_TIME_CALCULATION");
		goto out_err;
	case ReflexxesAPI::RML_ERROR_SYNCHRONIZATION:
		ERR("RML_ERROR_SYNCHRONIZATION");
		goto out_err;
	case ReflexxesAPI::RML_ERROR_NUMBER_OF_DOFS:
		ERR("RML_ERROR_NUMBER_OF_DOFS");
		goto out_err;
	case ReflexxesAPI::RML_ERROR_NO_PHASE_SYNCHRONIZATION:
		ERR("RML_ERROR_NO_PHASE_SYNCHRONIZATION");
		goto out_err;
	case ReflexxesAPI::RML_ERROR_NULL_POINTER:
		ERR("RML_ERROR_NULL_POINTER");
		goto out_err;
	case ReflexxesAPI::RML_ERROR_EXECUTION_TIME_TOO_BIG:
		ERR("RML_ERROR_EXECUTION_TIME_TOO_BIG");
		goto out_err;
	default:
		ERR("unkown error");
	}

	memcpy(cmd_pos, inf->OP->NewPositionVector->VecData, 5);
	memcpy(cmd_vel, inf->OP->NewVelocityVector->VecData, 5);
	memcpy(cmd_acc, inf->OP->NewAccelerationVector->VecData, 5);

	write_cmd_pos_5(inf->ports.cmd_pos, &cmd_pos);
	write_cmd_vel_5(inf->ports.cmd_vel, &cmd_vel);
	write_cmd_acc_5(inf->ports.cmd_acc, &cmd_acc);
out_err:
	return;
}
