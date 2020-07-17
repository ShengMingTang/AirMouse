import { func } from 'prop-types';

import {
    pushStart as pushStartFromApi, 
    pushEnd as pushEndFromApi,
    pushMore as pushMoreFromApi,
    bin2Base64,
    BLOCK_SIZE
} from 'api/common.js';

const seqBase = 0;
const seqMax = 20;

export function onChange(files){
    return (dispatch, getState) => {
        let reader = new FileReader();
        reader.addEventListener('load', function(filename, e){
            dispatch(onLoad(btoa(this.result), filename));
        }.bind(reader, files[0].name));
        reader.readAsBinaryString(files[0]);
    };
}

export function onLoad(file64, filename){
    return {
        type: '@FILESELECTOR/ONLOAD',
        file64,
        filename
    }
}

function pushStart(){
    return {
        type: '@FILESELECTOR/PUSH_START',
        status: 'start',
        seq: 0,
        offset: 0,
    };
}
function pushProgress(seq, offset){
    return {
        type: '@FILESELECTOR/PUSH_PROGRESS',
        status: 'pushing',
        seq,
        offset,
    }
}
function pushEnd(status){
    return {
        type: '@FILESELECTOR/PUSH_END',
        status
    };
}

export function onPush(file64, filename){
    return (dispatch, getState) => {
        dispatch(pushStart());
        return pushStartFromApi(filename).then((res) => {
            dispatch(pushMore(file64, 0, 0));
            // dispatch(pushEnd('success'));
        }).catch(() => {
            dispatch(pushEnd('fail'));
        });
    };
}
function pushMore(file64, seq, offset){
    const nextSeq = seqBase + (seq + 1) % seqMax;
    const nextOffset = offset + BLOCK_SIZE;

    return (dispatch, getState) => {
        if(offset < file64.length){ // load
            dispatch(pushProgress(nextSeq, nextOffset));
            return pushMoreFromApi(file64, seq, offset).then((res) => {
                dispatch(pushMore(file64, nextSeq, nextOffset));
            }).catch((err) => {
                dispatch(pushEnd(`Push Err ${err}`));
            });
        }
        else{ // end
            dispatch(pushClose(getState().filename));
        }
    };
}
function pushClose(filename){
    return (dispatch, getState) => {
        return pushEndFromApi(filename).then((res) => {
            dispatch(pushEnd('success'));
        }).catch((err) => {
            dispatch(pushEnd(`Push Close Err ${err}`));
        });
    }
}