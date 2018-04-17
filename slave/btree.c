query_result *btree_query_1_svc(btree_query_args args, struct svc_req *req)
{
    u_int64_t *result_vector;
    unsigned int result_vector_len;
    query_result *res;
    if (args.local_vectors.local_vectors_len == 0) { // this is the coordinator
        result_vector = NULL;
    }
    else {
        unsigned int num_local_vecs = args.local_vectors.local_vectors_len;
        // operate on local vectors
        unsigned int *local_vecs = args.local_vectors.local_vectors_val;
        res = get_vector(local_vecs[0]);
        result_vector = res->vector.vector_val;
        result_vector_len = res->vector.vector_len;
        int i;
        for (i = 1; i < num_local_vecs; i++) {
            char op = args.local_ops.local_ops_val[i - 1];
            query_result *second_vec_r = get_vector(local_vecs[i]);
            u_int64_t *second_vec = second_vec_r->vector.vector_val;
            unsigned int second_vec_len = second_vec_r->vector.vector_len;
            u_int64_t *new_result_vector = (u_int64_t *)
                malloc(sizeof(u_int64_t) * max(second_vec_len, result_vector_len));
            if (op == '&') {
                result_vector_len = AND_WAH(
                    new_result_vector, result_vector,
                    result_vector_len, second_vec,
                    second_vec_len);
            }
            else if (op == '|') {
                result_vector_len = OR_WAH(
                    new_result_vector, result_vector,
                    result_vector_len, second_vec,
                    second_vec_len);
            }
            free(result_vector);
            result_vector = new_result_vector;
        }
    }

    // the final call: no more vectors to visit
    if (args.recur_query_list_len == 0) {
        return res;
    }

    unsigned int sub_len = args.recur_query_list_len;
    query_result *result[sub_len];
    int i;
    for (i = 0; i < sub_len; i++) {
        btree_query_args a = args.recur_query_list[i];

        CLIENT *clnt = clnt_create(a.this_machine_address, BTREE_QUERY_PIPE,
            BTREE_QUERY_PIPE_V1, "tcp");
        if (clnt == NULL) {
            res->exit_code = EXIT_FAILURE;
            return res;
        }
        result[i] = btree_query_1(a, clnt);
        if (result[i] == NULL) {
            res->exit_code = EXIT_FAILURE;
            return res;
        }
    }
    // TODO: move to helper function.
    if (result_vector == NULL) {
        result_vector = result[0]->vector.vector_val;
        result_vector_len = result[0]->vector.vector_len;
        // XXX: it *should* be safe to and this first vector by itself,
        // but if it's not, then have the following for loop skip
        // the first iteration
    }
    for (i = 0; i < sub_len; i++) {
        char op = args.subquery_ops.subquery_ops_val[i];
        query_result *second_vec_r = result[i];
        u_int64_t *second_vec = second_vec_r->vector.vector_val;
        unsigned int second_vec_len = second_vec_r->vector.vector_len;
        u_int64_t *new_result_vector = (u_int64_t *)
            malloc(sizeof(u_int64_t) * max(second_vec_len, result_vector_len));
        if (op == '&') {
            result_vector_len = AND_WAH(
                new_result_vector, result_vector,
                result_vector_len, second_vec,
                second_vec_len);
        }
        else if (op == '|') {
            result_vector_len = OR_WAH(
                new_result_vector, result_vector,
                result_vector_len, second_vec,
                second_vec_len);
        }
        free(result_vector);
        result_vector = new_result_vector;
    }
    memcpy(res->vector.vector_val, result_vector, result_vector_len *
        sizeof(u_int64_t));
    res->vector.vector_len = result_vector_len;
    res->exit_code = EXIT_SUCCESS;
    res->error_message = "";
    return res;
}
