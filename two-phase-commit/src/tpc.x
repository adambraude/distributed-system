/*
 * XXX: instead of being able to send just one vector at a time,
 * send a linked list of vec_args structs
 */

struct commit_vec_args {
    unsigned int vec_id;
    unsigned hyper int vec;
};

program TWO_PHASE_COMMIT_VOTE {
    version TWO_PHASE_COMMIT_VOTE_V1 {
        int COMMIT_MSG() = 1;
    } = 1;
} = 0x00000001;

program TWO_PHASE_COMMIT_VEC {
    version TPC_COMMIT_VEC_V1 {
        int COMMIT_VEC(commit_vec_args) = 1;
    } = 1;
} = 0x00000002;
