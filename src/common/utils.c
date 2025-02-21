///////////////////////////////////////////////////////////////////////////////////
// File : utils.c
// Contains: utils functions/helpes.
//
// This file is part of SimpleRemoteDesktop.
//
// Written by: Jean-François DEL NERO
//
// Copyright (C) 2025 Jean-François DEL NERO
//
// Change History (most recent first):
///////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

char * curdatestr()
{
    char *foo;

    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );

    foo = asctime (timeinfo);
    foo[strlen(foo) - 1] = 0;
    return foo;
}

