// TODO: fn_name
    /*char *parts[10]; // TODO duh*/
    /*size_t size = 6; // fn + ( + ) + : + \0*/
    /*TypeList *_args = args;*/
    /*for (int i = 0; _args != NULL; i++) {*/
        /*parts[i] = _args->item->name;*/
        /*size += strlen(parts[i]);*/
        /*if (i < nargs - 1) {*/
            /*size += 1; // + ','*/
        /*}*/
        /*_args = _args->next;*/
    /*}*/
    /*char *retname = ret->name;*/
    /*size_t retlen = strlen(retname);*/
    /*size += retlen;*/
    /*if (variadic) {*/
        /*size += 3;*/
    /*}*/
    /*char *buf = malloc(sizeof(char) * size);*/
    /*strncpy(buf, "fn(", 3);*/
    /*char *m = buf + 3;*/
    /*_args = args;*/
    /*for (int i = 0; _args != NULL; i++) {*/
        /*size_t n = strlen(parts[i]);*/
        /*strncpy(m, parts[i], n);*/
        /*m += n;*/
        /*if (i < nargs - 1) {*/
            /*m[0] = ',';*/
            /*m += 1;*/
        /*}*/

        /*if (_args->item->base == FN_T) {*/
            /*free(parts[i]);*/
        /*}*/
        /*_args = _args->next;*/
    /*}*/
    /*if (variadic) {*/
        /*strncpy(m, "...", 3);*/
        /*m += 3;*/
    /*}*/
    /*strncpy(m, "):", 2);*/
    /*m += 2;*/
    /*strncpy(m, retname, retlen);*/
    /*if (ret->base == FN_T) {*/
        /*free(retname);*/
    /*}*/
    /*buf[size-1] = 0;*/

// TODO: struct name
    /*int named = (name != NULL);*/
    /*if (!named) {*/
        /*int len = 8;*/
        /*for (int i = 0; i < nmembers; i++) {*/
            /*len += strlen(member_types[i]->name);*/
            /*if (i > 0) {*/
                /*len += 2; // comma and space*/
            /*}*/
        /*}*/
        /*name = malloc(sizeof(char) * (len + 1));*/
        /*sprintf(name, "struct{");*/
        /*int c = 7;*/
        /*for (int i = 0; i < nmembers; i++) {*/
            /*if (i > 0) {*/
                /*sprintf(name + (c++), ",");*/
            /*}*/
            /*sprintf(name + c, "%s", member_types[i]->name);*/
            /*c += strlen(member_types[i]->name);*/
        /*}*/
        /*sprintf(name + c, "}");*/
        /*name[len] = 0;*/
    /*}*/
