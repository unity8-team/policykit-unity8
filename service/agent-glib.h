
#pragma once

#include <polkitagent/polkitagent.h>

#include "agent.h"

typedef struct _AgentGlib AgentGlib;
typedef struct _AgentGlibClass AgentGlibClass;

GType agent_glib_get_type(void) G_GNUC_CONST;
AgentGlib* agent_glib_new(Agent* parent);
