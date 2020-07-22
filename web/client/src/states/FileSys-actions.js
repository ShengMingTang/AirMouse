import { func } from 'prop-types';

import {
    bin2Base64 as bin2Base64FromApi,
    fileReaderHelper as fileReaderHelperFromApi,

    pushStart as pushStartFromApi,
    pushMore as pushMoreFromApi,
    pushEnd as pushEndFromApi,
    
    base64ToBin as base64ToBinFromApi,
    createDownloader as createDownloaderFromApi,
    createAndDownloadBlobFile as createAndDownloadBlobFileFromApi,
    downloadByUrl as downloadByUrlFromApi,

    pullStart as pullStartFromApi,
    pullMore as pullMoreFromApi,
    pullEnd as pullEndFromApi,
} from 'api/common.js';

// ************ important **************/
// cancel on loading object should be well errorhandled
// ************ important **************/

// ************ important **************/
// remember to switch #debug
// ************ important **************/
const debug = 1;

/* handle trigger from UI*/
export function onDownload(path, url){
    return (dispatch, getState) => {
        if(url === undefined){ // pull
            dispatch(addToPullWork(path));
            triggerPullWork();
        }
        else{ // direct down load
            downloadByUrlFromApi(url);
        }
    };
}
export function onRemovePullWork(work){
    return (dispatch, getState) => {
        // work = {path, file, status}
        const {path, file64, status} = work;
        // loading, alert user
        if(status === 'loading'){
            debug&&console.log('alert canceling');
        }
        else{ // safely remove from queue
            dispatch(removeFromPullWork(path));
        }
    };
}
export function onCancelPull(path){
    return {
        type: '@FILESYS/CANCEL_PULL',
        path
    };
}

/* simple action for workers */
function pullWorkerSet(staus){ // set pullWorker to status
    return {
        type: '@FILESYS/PULL_WORKER_SET',
        status
    };
}
function addToPullWork(path){ // add file to pullWork queue
    return {
        type: '@FILESYS/ADD_TO_PULL_WORK',
        path,
        file64: '',
        status: 'waiting'
    };
}
export function removeFromPullWork(path){
    return {
        type: '@FILESYS/REMOVE_FROM_PULL_WORK',
        path
    };
}
function pullWorkStart(){ // pullWorker will get its job
    return {
        type: '@FILESYS/PULL_WORK_START',
    };
}


/* composite action for workers */
function triggerPullWork(){ // pusllWorker get a job -> init pull (onPull)
    return (dispatch, getState) => {
        if(getState().fileSys.pullWorks.length && getState().fileSys.pullWorker.status === 'idle'){
            dispatch(pullWorkStart());
            const path = getState().fileSys.pushWorker.path;
            dispatch(onPull(path));
        }
    };
}

function pullProgress(isReady, data){
    return {
        type: '@FILESYS/PULL_PROGRESS',
        data
    };
}

export function onPull(filename){
    return (dispatch, getState) => {
        return pullStartFromApi(filename).then(res => {
            dispatch(pullMore());
        }).catch((err) => {
            dispatch(pullWorkerSet('failed'));
        });
    };
}

function pullMore(){
    return (dispatch, getState) => {
        return pullMoreFromApi().then(res => {
            let data = res.querySelector('#pullData');
            
            if(data !== ''){ // pull more
                dispatch(pullProgress(true, data));
                dispatch(pullMore());
            }
            else{ // end
                const {file64, filename} = getState().fileSys;
                dispatch(pullClose());
            }
        })
    };
}
function pullClose(filename){
    return (dispatch, getState) => {
        return pullEndFromApi(filename).then((res) => {
            const {file64, path} = getState().fileSys.pullWorker;
            let filename = getState().fileSys.pullWorker.path;
            filename = filename.split('/');
            filename = filename[filename.length - 1]; // /d1/d2/.../filename.ext
            const url = createAndDownloadBlobFileFromApi(base64ToBinFromApi(file64), filename);
            
            dispatch(addToCache(path, url));
            dispatch(pullWorkerSet('idle'));
            (!debug)&&triggerPullWork();

        }).catch((err) => {
            dispatch(pullWorkerSet(`Push Close Err ${err}`));
        });
    }
}
function addToCache(path, url){ // add url to the node

}


/* call flow: 
    enqueue
    onChange... -> onLoad... -> addToPushWork [-> deque
    
    deque                    pushing
    -> triggerPushWork... -> onPush...
                                        -> pushMore
                                        -> pushClose -> pushEnd -> triggerPushWork
*/

/* handle file reader and trigger from UI*/
export function onChange(files, path){
    if(files){
        return (dispatch, getState) => {
            let prms = [];
            for(let i = 0; i < files.length; i++){
                let fullPath = (path ? path : '/') + files[i].name;
                prms.push(fileReaderHelperFromApi(fullPath, files[i]));
            }
            return Promise.all(prms).then(filePacks => {
                // [{file64, path}...]
                console.log(filePacks);
                dispatch(addToPushWork(filePacks));
                (!debug)&&triggerPushWork();
            }).catch(err => {
                console.log(err);
            });
        };
        // return (dispatch, getState) => {
        //     for(let i = 0; i < files.length; i++){
        //         let reader = new FileReader();
        //         reader.addEventListener('load', function(filename, e){
        //             dispatch(onLoad((path ? path : '/') + filename, this.result));
        //         }.bind(reader, files[i].name));
        //         reader.readAsBinaryString(files[i]);
        //     }

        // };
    }
}
export function removeFromPushWork(path){
    return {
        type: '@FILESYS/REMOVE_FROM_PUSH_WORK',
        path
    };
}
export function onCancelPush(path){
    return {
        type: '@FILESYS/CANCEL_PUSH',
        path
    };
}

/* simple action for workers */
function pushWorkerSet(staus){ // set pushWorker to status
    return {
        type: '@FILESYS/PUSH_WORKER_SET',
        status
    };
}
function addToPushWork(filePacks){ // add file to pushWork queue
    return {
        type: '@FILESYS/ADD_TO_PUSH_WORK',
        filePacks: filePacks.map(p => (
            {...p, status: 'waiting'}
        ))
    };
}
function pushWorkStart(){ // pushWorker will get its job
    return {
        type: '@FILESYS/PUSH_WORK_START',
    };
}
/* composite action for workers */
function triggerPushWork(){ // pushWorker get a job -> init push(onPush)
    return (dispatch, getState) => {
        if(getState().fileSys.pushWorks.length && getState().fileSys.pushWorker.status === 'idle'){
            dispatch(pushWorkStart());
            const file64 = getState().fileSys.pushWorker.file64;
            const path = getState().fileSys.pushWorker.path;
            dispatch(onPush(file64, path));
        }
    };
}

/* actions for pushing in details */
function onPush(file64, filename){ // trigger a series of push operation
    return (dispatch, getState) => {
        return pushStartFromApi(filename).then((res) => {
            dispatch(pushMore(file64, 0, 0));
        }).catch(() => {
            dispatch(pushWorkerSet('fail'));
        });
    };
}
function pushProgress(seq, offset){
    return {
        type: '@FILESYS/PUSH_PROGRESS',
        status: 'pushing',
        seq,
        offset,
    }
}

function pushMore(file64, seq, offset){

    return (dispatch, getState) => {
        if(offset < file64.length){ // load
            dispatch(pushProgress(seq, offset + BLOCK_SIZE));
            return pushMoreFromApi(file64, seq, offset).then((res) => {
                dispatch(pushMore(file64, seqBase + (seq + 1) % seqMax, offset + BLOCK_SIZE));
            }).catch((err) => {
                dispatch(pushWorkerSet(`Push Err ${err}`));
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
            dispatch(pushWorkerSet('idle'));
            triggerPushWork();
        }).catch((err) => {
            dispatch(pushWorkerSet(`Push Close Err ${err}`));
        });
    }
}