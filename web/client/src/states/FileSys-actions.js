import { func } from 'prop-types';

import {
    BLOCK_SIZE,

    bin2Base64 as bin2Base64FromApi,
    fileReaderHelper as fileReaderHelperFromApi,
    listCc3200Fs as listCc3200FsFromApi,

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
const debug = 0;

export function listFs(){
    return (dispatch, getState) => {
        return listCc3200FsFromApi().then(tree => {
            dispatch(updateFs(tree));
        });
    };
}
function updateFs(tree){
    return {
        type: '@FILESYS/UPDATE_FS',
        tree
    };
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
export function onChange(files, parent){
    if(files){
        return (dispatch, getState) => {
            let prms = [];
            for(let i = 0; i < files.length; i++){
                let fullPath = (parent === '/' ? parent : parent + '/') + files[i].name;
                // fullPath will be truncated to 8.3 fmt
                prms.push(fileReaderHelperFromApi(fullPath, files[i]));
            }
            return Promise.all(prms).then(filePacks => {
                // [{file64, path}...]
                dispatch(addToPushWork(filePacks));
                dispatch(triggerPushWork());
            }).catch(err => {
                pushWorkerSet(`File Onchange err with ${err}`);
            });
        };
    }
}
export function removeFromPushWork(path){
    return {
        type: '@FILESYS/REMOVE_FROM_PUSH_WORK',
        path
    };
}
export function onCancelPush(path){
    return pushWorkSet(path, 'canceled');
}

/* simple action for workers */
function pushWorkerSet(status){ // set pushWorker to status
    return {
        type: '@FILESYS/PUSH_WORKER_SET',
        status
    };
}
function pushWorkSet(path, status){ // set a work to status
    return {
        type: '@FILESYS/PUSH_WORK_SET',
        path,
        status,
    }
}
function addToPushWork(filePacks){ // add file to pushWork queue
    return {
        type: '@FILESYS/ADD_TO_PUSH_WORK',
        filePacks: filePacks.map(p => (
            {...p, status: 'waiting'}
        ))
    };
}
function pushWorkerGetJob(idx){ // pushWorker will get its job
    return {
        type: '@FILESYS/PUSH_WORKER_GET_JOB',
        workIdx: idx
    };
}
/* composite action for workers */
function triggerPushWork(){ // pushWorker get a job -> init push(onPush)
    return (dispatch, getState) => {
        const {pushWorks} = getState().fileSys;
        let idx = -1;
        
        for(let i = 0; i < pushWorks.length; i++){
            if(pushWorks[i].status === 'waiting'){
                idx = i;
                break;
            }
        }
        if(idx !== -1 && getState().fileSys.pushWorker.status === 'idle'){ // valid
            dispatch(pushWorkerGetJob(idx));
            const {file64, path} = getState().fileSys.pushWorker;
            dispatch(pushWorkSet(path, 'loading'));
            dispatch(onPush(file64, path));
        }
    };
}

/* actions for pushing in details */
function onPush(file64, filename){ // trigger a series of push operation
    return (dispatch, getState) => {
        return pushStartFromApi(filename).then((res) => {
            dispatch(pushMore(file64, 0));
        }).catch((err) => {
            dispatch(pushWorkerSet(`Push Start failed with ${err}`));
        });
    };
}
function pushProgress(offset){
    return {
        type: '@FILESYS/PUSH_PROGRESS',
        status: 'pushing',
        offset,
    }
}

function pushMore(file64, offset = 0){

    return (dispatch, getState) => {
        if(offset < file64.length){ // load
            return pushMoreFromApi(file64, offset).then((res) => {
                const {status, offsetNext} = res;
                dispatch(pushMore(file64, offsetNext));
                dispatch(pushProgress(offsetNext));
            }).catch((err) => {
                dispatch(pushWorkerSet(`Push Err with ${err}`));
            });
        }
        else{ // end
            dispatch(pushClose(getState().fileSys.pushWorker.path));
        }
    };
}
function pushClose(filename){
    return (dispatch, getState) => {
        return pushEndFromApi(filename).then((res) => {
            dispatch(pushWorkerSet('idle'));
            dispatch(pushWorkSet(filename, 'done'));
            setTimeout(() => {
                dispatch(triggerPushWork());
            }, 500);
        }).catch((err) => {
            dispatch(pushWorkerSet(`Push Close Err ${err}`));
        });
    }
}












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
    return pullWorkSet(path, 'canceled');
}

/* simple action for workers */
function pullWorkerSet(staus){ // set pullWorker to status
    return {
        type: '@FILESYS/PULL_WORKER_SET',
        status
    };
}
function pullWorkSet(path, status){ // set a work to status
    return {
        type: '@FILESYS/PULL_WORK_SET',
        path,
        status,
    }
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
            dispatch(pullWorkerSet(err));
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
                dispatch(pullClose(filename));
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
            dispatch(pullWorkSet(path, 'done'));

            (!debug)&&triggerPullWork();

        }).catch((err) => {
            dispatch(pullWorkerSet(`Push Close Err ${err}`));
        });
    }
}
function addToCache(path, url){ // add url to the node

}