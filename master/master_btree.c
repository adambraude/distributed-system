btree_query_args *get_query_args(PyObject *);

void _recur_print(btree_query_args *args)
{
    printf("At machine %s\n", args->this_machine_address);
        int i;

    if (args->local_vectors.local_vectors_val != NULL) {
        puts("for loop");
        for (i = 0; i < args->local_vectors.local_vectors_len; i++) {
            printf("Lv %d = %d\n", i, args->local_vectors.local_vectors_val[i]);
        }
    }
    puts("1st if");
    if (args->recur_query_list_len == 0) {
        puts("Done.");
        return;
    }
    puts("recursive print");
    for (i = 0; i < args->recur_query_list_len; i++) {
        puts("nul chck");
        //printf("is null: %d\n", args->recur_query_list[i] == NULL);
        _recur_print(&args->recur_query_list[i]);
    }
}

int init_btree_range_query(range_query_contents contents)
{
    Py_Initialize();
    PyImport_ImportModule("heapq");
    PySys_SetPath(".");
    // TODO: condition on plan type
    PyObject *pModule, *pValue;
    pModule = PyImport_ImportModule("mst_planner");
    if (pModule != NULL) {
        PyObject *pyArg;
        pyArg = PyList_New(contents.num_ranges);
        // set the subranges
        int i;
        for (i = 0; i < contents.num_ranges; i++) {
            int range_len = contents.ranges[i][1] - contents.ranges[i][0] + 1;
            PyObject *sublist = PyList_New(range_len);
            PyList_SetItem(pyArg, i, sublist);;
            //unsigned int vec_ids[range_len];
            int j;
            for (j = contents.ranges[i][0]; j <= contents.ranges[i][1]; j++) {
                PyObject *t1, *t2;
                t1 = PyTuple_New(2);
                t2 = PyTuple_New(replication_factor);
                PyTuple_SetItem(t1, 0, PyInt_FromLong(j));
                PyTuple_SetItem(t1, 1, t2);
                unsigned int *tup = get_machines_for_vector(j, false);
                PyTuple_SetItem(t2, 0, PyInt_FromLong(tup[0]));
                PyTuple_SetItem(t2, 1, PyInt_FromLong(tup[1]));
                PyList_SetItem(sublist, j - contents.ranges[i][0], t1);
            }
        }
        PyObject *pFunc = PyObject_GetAttrString(pModule, "iter_mst");
        if (pFunc != NULL && PyCallable_Check(pFunc)) {
            pValue = PyObject_CallFunctionObjArgs(pFunc, pyArg, NULL);
            if (pValue != NULL) {
                PyObject *res = PyObject_CallFunctionObjArgs(pFunc, pyArg, NULL);
                // res is of the form: (Coordinator, Trees)
                long coordinator_node = PyLong_AsLong(PyTuple_GetItem(res, 0));
                PyObject *trees = PyTuple_GetItem(res, 1);
                int num_trees = PyList_Size(trees);
                int k;
                btree_query_args *args[num_trees];
                for (k = 0; k < num_trees; k++) {
                    PyObject *query_tree = PyList_GetItem(trees, k);
                    args[k] = get_query_args(query_tree);
                }
                btree_query_args *coordinator_args = (btree_query_args *)
                    malloc(sizeof(btree_query_args));
                char addr[32];
                coordinator_args->this_machine_address = addr;
                strcpy(coordinator_args->this_machine_address, SLAVE_ADDR[coordinator_node - 1]);
                coordinator_args->recur_query_list = args;
                coordinator_args->recur_query_list_len = num_trees;
                int num_coord_ops = num_trees + 1; // XXX may be off-by-1
                char coord_ops[num_coord_ops];
                coordinator_args->subquery_ops.subquery_ops_val = coord_ops;
                memset(coordinator_args->subquery_ops.subquery_ops_val, '&', num_coord_ops);
                coordinator_args->subquery_ops.subquery_ops_len = num_coord_ops;
                _recur_print(coordinator_args);
                CLIENT *clnt = clnt_create(coordinator_args->this_machine_address,
                    BTREE_QUERY_PIPE, BTREE_QUERY_PIPE_V1, "tcp");
                btree_query_1(*coordinator_args, clnt);
            }
            else
                PyErr_Print();
        }
        else
            PyErr_Print();
    }
    else {
        PyErr_Print();
    }
    Py_Finalize();
    return 0;
}

btree_query_args *get_query_args(PyObject *vertex)
{
    PyObject *children = PyObject_GetAttrString(vertex, "children"); // list of Vertex objects
    btree_query_args *args = (btree_query_args *)
        malloc(sizeof(btree_query_args));
    PyObject *vectors = PyObject_GetAttrString(vertex, "vectors"); // list of integers (machine IDs)
    unsigned int num_vecs = PyList_Size(vectors);
    unsigned int arr[num_vecs];
    char ops[num_vecs - 1];
    args->local_vectors.local_vectors_val = arr;
    args->local_vectors.local_vectors_len = num_vecs;
    args->local_ops.local_ops_val = ops;
    args->local_ops.local_ops_len = num_vecs - 1;
    memset(args->local_ops.local_ops_val, '|', num_vecs - 1);
    long machine_id = PyInt_AsLong(PyObject_GetAttrString(vertex, "id"));
    strcpy(args->this_machine_address, SLAVE_ADDR[machine_id - 1]);
    int j;
    for (j = 0; j < num_vecs; j++) {
        args->local_vectors.local_vectors_val[j] = PyLong_AsLong(PyList_GetItem(vectors, j));
    }
    int num_children = PyList_Size(children);
    if (num_children == 0) { // base case
        //args->recur_query_list = NULL; // XXX why did I do this...
        return args;
    }
    btree_query_args subqs[num_children];
    args->recur_query_list = subqs;
    args->recur_query_list_len = num_children;
    char subops[num_children - 1];
    args->subquery_ops.subquery_ops_val = subops;
    args->subquery_ops.subquery_ops_len = num_vecs - 1;
    memset(args->subquery_ops.subquery_ops_val, '|', num_vecs - 1);
    int i;
    for (i = 0; i < num_children; i++) {
        args->recur_query_list[i] = *get_query_args(PyList_GetItem(children, i));
    }
    return args;
}
