/* 
 * Reflexxes based trajectory generator block.
 */

/* #define DEBUG */
#include "rml_pos.hpp"

UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)

#include <ReflexxesAPI.h>
#include <RMLPositionFlags.h>
#include <RMLPositionInputParameters.h>
#include <RMLPositionOutputParameters.h>

/* define a structure for holding the block local state. By assigning an
 * instance of this struct to the block private_data pointer (see init), this
 * information becomes accessible within the hook functions.
 */
struct rml_pos_info
{
	ReflexxesAPI *RML;
	RMLPositionInputParameters *IP;
	RMLPositionOutputParameters *OP;
	RMLPositionFlags Flags;

	/* this is to have fast access to ports for reading and writing, without
	 * needing a hash table lookup */
	struct rml_pos_port_cache ports;
};

/* init */
int rml_pos_init(ubx_block_t *b)
{
	int ret = -1;
	unsigned int len;
	struct rml_pos_info *inf;
	double cycle_time;

	/* allocate memory for the block local state */
	if ((inf = (rml_pos_info*) calloc(1, sizeof(struct rml_pos_info)))==NULL) {
		ERR("rml_pos: failed to alloc memory");
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
	inf->Flags.SynchronizationBehavior = RMLPositionFlags::ONLY_TIME_SYNCHRONIZATION;

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
int rml_pos_start(ubx_block_t *b)
{
	int ret = -1;
	ubx_data_t *max_vel_data, *max_acc_data;

	struct rml_pos_info *inf = (struct rml_pos_info*) b->private_data;

	max_vel_data = ubx_config_get_data(b, "max_vel");
	if(max_vel_data->len != 5) {
		ERR("invalid array dimensions of config max_vel. is %lu but should be %d", max_vel_data->len, 5);
		goto out;
	}
	memcpy(inf->IP->MaxVelocityVector->VecData, max_vel_data->data, sizeof(double[5]));

	DBG("setting max_vel: [%f, %f, %f, %f, %f]",
	    inf->IP->MaxVelocityVector->VecData[0],
	    inf->IP->MaxVelocityVector->VecData[1],
	    inf->IP->MaxVelocityVector->VecData[2],
	    inf->IP->MaxVelocityVector->VecData[3],
	    inf->IP->MaxVelocityVector->VecData[4]);

	max_acc_data = ubx_config_get_data(b, "max_acc");
	if(max_acc_data->len != 5) {
		ERR("invalid array dimensions of config max_acc. is %lu but should be %d", max_acc_data->len, 5);
		goto out;
	}
	memcpy(inf->IP->MaxAccelerationVector->VecData, max_acc_data->data, sizeof(double[5]));

	DBG("setting max_acc: [%f, %f, %f, %f, %f]",
	    inf->IP->MaxAccelerationVector->VecData[0],
	    inf->IP->MaxAccelerationVector->VecData[1],
	    inf->IP->MaxAccelerationVector->VecData[2],
	    inf->IP->MaxAccelerationVector->VecData[3],
	    inf->IP->MaxAccelerationVector->VecData[4]);

	for(int i=0; i<5; i++) {
		/* not supported in LGPL version anyway? */
		inf->IP->MaxJerkVector->VecData[i]=100;

		inf->IP->CurrentPositionVector->VecData[i]=0;
		inf->IP->CurrentVelocityVector->VecData[i]=0;
		inf->IP->CurrentAccelerationVector->VecData[i]=0;
	}

	ret = 0;
 out:
	return ret;
}

/* stop */
void rml_pos_stop(ubx_block_t *b)
{
	/* struct _info *inf = (struct _info*) b->private_data; */
}

/* cleanup */
void rml_pos_cleanup(ubx_block_t *b)
{
	struct rml_pos_info *inf;
	inf = (struct rml_pos_info*) b->private_data;

	delete inf->RML;
	delete inf->IP;
	delete inf->OP;

	free(inf);
}

/* step */
void rml_pos_step(ubx_block_t *b)
{
	int i, res;
	double tmparr[5];
	double cmd_pos[5], cmd_vel[5], cmd_acc[5];
	int sz_msr_pos, sz_msr_vel, sz_des_pos, sz_des_vel;

	struct rml_pos_info *inf = (struct rml_pos_info*) b->private_data;

	/* new target pos? */
	sz_des_pos = read_des_pos_5(inf->ports.des_pos, &tmparr);
	if(sz_des_pos == 5) {
		i=0; /* reuse i to emit "reached" event */
		write_reached(inf->ports.reached, &i);
		memcpy(inf->IP->TargetPositionVector->VecData, tmparr, sizeof(tmparr));

		DBG("des_pos: [%f, %f, %f, %f, %f]",
		    inf->IP->TargetPositionVector->VecData[0],
		    inf->IP->TargetPositionVector->VecData[1],
		    inf->IP->TargetPositionVector->VecData[2],
		    inf->IP->TargetPositionVector->VecData[3],
		    inf->IP->TargetPositionVector->VecData[4]);
	}

	/* new target vel? */
	sz_des_vel = read_des_vel_5(inf->ports.des_vel, &tmparr);
	if(sz_des_vel == 5) {
		i=0; /* reuse i to emit "reached" event */
		write_reached(inf->ports.reached, &i);
		memcpy(inf->IP->TargetVelocityVector->VecData, tmparr, sizeof(tmparr));

		DBG("des_vel: [%f, %f, %f, %f, %f]",
		    inf->IP->TargetVelocityVector->VecData[0],
		    inf->IP->TargetVelocityVector->VecData[1],
		    inf->IP->TargetVelocityVector->VecData[2],
		    inf->IP->TargetVelocityVector->VecData[3],
		    inf->IP->TargetVelocityVector->VecData[4]);
	}

	/* new measured pos? */
	sz_msr_pos = read_msr_pos_5(inf->ports.msr_pos, &tmparr);
	if(sz_msr_pos == 5) {
		memcpy(inf->IP->CurrentPositionVector->VecData, tmparr, sizeof(tmparr));
		DBG("msr_pos: [%f, %f, %f, %f, %f]",
		    inf->IP->CurrentPositionVector->VecData[0],
		    inf->IP->CurrentPositionVector->VecData[1],
		    inf->IP->CurrentPositionVector->VecData[2],
		    inf->IP->CurrentPositionVector->VecData[3],
		    inf->IP->CurrentPositionVector->VecData[4]);
	}

	/* new measured vel? */
	sz_msr_vel = read_msr_vel_5(inf->ports.msr_vel, &tmparr);
	if(sz_msr_vel == 5) {
		memcpy(inf->IP->CurrentVelocityVector->VecData, tmparr, sizeof(tmparr));

		DBG("msr_vel: [%f, %f, %f, %f, %f]",
		    inf->IP->CurrentVelocityVector->VecData[0],
		    inf->IP->CurrentVelocityVector->VecData[1],
		    inf->IP->CurrentVelocityVector->VecData[2],
		    inf->IP->CurrentVelocityVector->VecData[3],
		    inf->IP->CurrentVelocityVector->VecData[4]);
	}


	/* select all DOF's */
	for(i=0; i<5; i++)
		inf->IP->SelectionVector->VecData[i] = true;


	DBG("CheckForValidity: %d", inf->IP->CheckForValidity());

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
		goto out_err;
	}

	memcpy(cmd_pos, inf->OP->NewPositionVector->VecData, sizeof(cmd_pos));
	memcpy(cmd_vel, inf->OP->NewVelocityVector->VecData, sizeof(cmd_vel));
	memcpy(cmd_acc, inf->OP->NewAccelerationVector->VecData, sizeof(cmd_acc));

	DBG("cmd_pos: [%f, %f, %f, %f, %f]",
	    inf->OP->NewPositionVector->VecData[0],
	    inf->OP->NewPositionVector->VecData[1],
	    inf->OP->NewPositionVector->VecData[2],
	    inf->OP->NewPositionVector->VecData[3],
	    inf->OP->NewPositionVector->VecData[4]);

	DBG("cmd_vel: [%f, %f, %f, %f, %f]",
	    inf->OP->NewVelocityVector->VecData[0],
	    inf->OP->NewVelocityVector->VecData[1],
	    inf->OP->NewVelocityVector->VecData[2],
	    inf->OP->NewVelocityVector->VecData[3],
	    inf->OP->NewVelocityVector->VecData[4]);

	DBG("cmd_acc: [%f, %f, %f, %f, %f]",
	    inf->OP->NewAccelerationVector->VecData[0],
	    inf->OP->NewAccelerationVector->VecData[1],
	    inf->OP->NewAccelerationVector->VecData[2],
	    inf->OP->NewAccelerationVector->VecData[3],
	    inf->OP->NewAccelerationVector->VecData[4]);

	write_cmd_pos_5(inf->ports.cmd_pos, &cmd_pos);
	write_cmd_vel_5(inf->ports.cmd_vel, &cmd_vel);
	write_cmd_acc_5(inf->ports.cmd_acc, &cmd_acc);
 out_err:
	return;
}
