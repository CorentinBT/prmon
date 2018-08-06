#include "cgapi.h"

int Cgroup::create_group(const std::string& controller){

	uid_t tuid = CGRULE_INVALID, auid = CGRULE_INVALID;
	gid_t tgid = CGRULE_INVALID, agid = CGRULE_INVALID;

	struct cgroup_group_spec **cgroup_list;
	struct cgroup *cgroup;
	struct cgroup_controller *cgc;

	/* approximation of max. numbers of groups that will be created */
	int capacity = argc;

	/* permission variables */
	mode_t dir_mode = NO_PERMS;
	mode_t file_mode = NO_PERMS;
	mode_t tasks_mode = NO_PERMS;
	int dirm_change = 0;
	int filem_change = 0;

	/* no parametr on input */
	if (argc < 2) {
		usage(1, argv[0]);
		return -1;
	}
	cgroup_list = calloc(capacity, sizeof(struct cgroup_group_spec *));
	if (cgroup_list == NULL) {
		fprintf(stderr, "%s: out of memory\n", argv[0]);
		ret = -1;
		goto err;
	}

	/* parse arguments */
	while ((c = getopt_long(argc, argv, "a:t:g:hd:f:s:", long_opts, NULL))
		> 0) {
		switch (c) {
		case 'h':
			usage(0, argv[0]);
			ret = 0;
			goto err;
		case 'a':
			/* set admin uid/gid */
			if (parse_uid_gid(optarg, &auid, &agid, argv[0]))
				goto err;
			break;
		case 't':
			/* set task uid/gid */
			if (parse_uid_gid(optarg, &tuid, &tgid, argv[0]))
				goto err;
			break;
		case 'g':
			ret = parse_cgroup_spec(cgroup_list, optarg, capacity);
			if (ret) {
				fprintf(stderr, "%s: "
					"cgroup controller and path"
					"parsing failed (%s)\n",
					argv[0], argv[optind]);
				ret = -1;
				goto err;
			}
			break;
		case 'd':
			dirm_change = 1;
			ret = parse_mode(optarg, &dir_mode, argv[0]);
			if (ret)
				goto err;
			break;
		case 'f':
			filem_change = 1;
			ret = parse_mode(optarg, &file_mode, argv[0]);
			if (ret)
				goto err;
			break;
		case 's':
			filem_change = 1;
			ret = parse_mode(optarg, &tasks_mode, argv[0]);
			if (ret)
				goto err;
			break;
		default:
			usage(1, argv[0]);
			ret = -1;
			goto err;
		}
	}

	/* no cgroup name */
	if (argv[optind]) {
		fprintf(stderr, "%s: "
			"wrong arguments (%s)\n",
			argv[0], argv[optind]);
		ret = -1;
		goto err;
	}

	/* initialize libcg */
	ret = cgroup_init();
	if (ret) {
		fprintf(stderr, "%s: "
			"libcgroup initialization failed: %s\n",
			argv[0], cgroup_strerror(ret));
		goto err;
	}

	/* for each new cgroup */
	for (i = 0; i < capacity; i++) {
		if (!cgroup_list[i])
			break;

		/* create the new cgroup structure */
		cgroup = cgroup_new_cgroup(cgroup_list[i]->path);
		if (!cgroup) {
			ret = ECGFAIL;
			fprintf(stderr, "%s: can't add new cgroup: %s\n",
				argv[0], cgroup_strerror(ret));
			goto err;
		}

		/* set uid and gid for the new cgroup based on input options */
		ret = cgroup_set_uid_gid(cgroup, tuid, tgid, auid, agid);
		if (ret)
			goto err;

		/* add controllers to the new cgroup */
		j = 0;
		while (cgroup_list[i]->controllers[j]) {
			cgc = cgroup_add_controller(cgroup,
				cgroup_list[i]->controllers[j]);
			if (!cgc) {
				ret = ECGINVAL;
				fprintf(stderr, "%s: "
					"controller %s can't be add\n",
					argv[0],
					cgroup_list[i]->controllers[j]);
				cgroup_free(&cgroup);
				goto err;
			}
			j++;
		}

		/* all variables set so create cgroup */
		if (dirm_change | filem_change)
			cgroup_set_permissions(cgroup, dir_mode, file_mode,
					tasks_mode);
		ret = cgroup_create_cgroup(cgroup, 0);
		if (ret) {
			fprintf(stderr, "%s: "
				"can't create cgroup %s: %s\n",
				argv[0], cgroup->name, cgroup_strerror(ret));
			cgroup_free(&cgroup);
			goto err;
		}
		cgroup_free(&cgroup);
	}
err:
	if (cgroup_list) {
		for (i = 0; i < capacity; i++) {
			if (cgroup_list[i])
				cgroup_free_group_spec(cgroup_list[i]);
		}
		free(cgroup_list);
	}
	return ret;
}

int Cgroup::delete_group(const std::string& controller){
  return system(("cgdelete -g " + controller + ":" + name).c_str());
}

int Cgroup::assign_proc_group(const std::string& controller, int pid){
  return system(("cgclassify -g " + controller + ":" + name + " " + std::to_string(pid)).c_str());
}

int Cgroup::set_value_group(const std::string& val_name, const std::string& value){
  return system(("cgset -r " + val_name + "=" + value + " " + name).c_str());
}
