#include "quakedef.h"


void SEG_CheckTouch (edict_t *e1, edict_t *e2)
{
	qboolean	*touched_p;
	int			ent_idx;
	float		segment_duration;

	ent_idx = EDICT_TO_PROG(e2) / sizeof(edict_t);
	touched_p = &sv.touched_edicts[ent_idx];

	if (!*touched_p &&
		Q_strcmp(PR_GetString(e1->v.classname), "player") == 0 &&
		(//Q_strcmp(PR_GetString(e2->v.classname), "") == 0 ||
		 Q_strcmp(PR_GetString(e2->v.classname), "func_button") == 0 ||
		 Q_strcmp(PR_GetString(e2->v.classname), "trigger_once") == 0 ||
		 Q_strcmp(PR_GetString(e2->v.classname), "trigger_multiple") == 0)) {

		segment_duration = sv.time - sv.last_segment_time;
		if (sv.time > 4.5 && segment_duration > 2.) {
			Con_Printf("Segment %s/%d: %.2fs %s %s\n", 
					   PR_GetString(e2->v.classname),
					   ent_idx,
					   segment_duration,
					   PR_GetString(e2->v.target),
					   PR_GetString(e2->v.targetname)
					   );
			sv.last_segment_time = sv.time;
		}

		*touched_p = 1;
	}
}

