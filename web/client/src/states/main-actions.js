import {postTest, lookTest} from 'api/common.js';

function status(sts){
    console.log(sts);
    return {
        type: '@MAIN/STATUS',
        status: sts
    }
}

export function test(){
    return (dispatch, getState) => {
        return postTest().then(res => {
            dispatch(status(res.toString()));
        }).catch(err => {
            dispatch(status(err.toString()));
        });
        // dispatch(status(getState().main.test))
    };
}