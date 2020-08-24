import {createDownloader as createDownloaderFromApi} from 'api/common.js';

const initFileSysState = {
    pushWorker: {
      status: 'idle', // 'idle'|'working'
      path: '',
      file64: '',
      offset: 0,
    },
    pushWorks: [
      // {path: '/temp/aa.txt', file64: '', status: 'loading'},
      // {path: 'cc.txt', file64: '', status: 'waiting'}
    ],

    pullWorker: {
      status: 'idle', // 'idle'|'working'
      path: '',
      file64: '',
    },
    pullWorks: [ // {path}
      // {path: '/temp/a.txt', file64: '', status: 'loading'},
      // {path: '/temp/b.txt', file64: '', status: 'waiting'},
      // {path: '/temp/c.txt', file64: '', status: 'canceled'},
      // {path: '/temp/d.txt', file64: '', status: 'done'},
    ],
    tree: {},
    // tree: {
    //   label: '/',
    //   parent: '',
    //   children: [
    //     {
    //       label: 'temp/',
    //       parent: '/',
    //       children: [
    //         {
    //           label: 'a.txt',
    //           parent: '/temp/',
    //         },
    //       ]
    //     },
    //     {
    //       label: 'test/',
    //       parent: '/',
    //     },
    //     {
    //       label: 'small.txt',
    //       parent: '/',
    //     }
    //   ]
    // },
};

export function fileSys(state = initFileSysState, action){
    switch(action.type){
        case '@FILESYS/UPDATE_FS':
          return {
            ...state,
            tree: action.tree
          };
        case '@FILESYS/PUSH_WORKER_SET':
          return {
            ...state,
            pushWorker: {...state.pushWorker, status: action.status}
          };
        case '@FILESYS/PUSH_WORK_SET':
          return {
            ...state,
            pushWorks: state.pushWorks.map(p => {
              if(p.path === action.path){
                return {...p, status: action.status};
              }
              else{
                return p;
              }
            })
          };
        case '@FILESYS/ADD_TO_PUSH_WORK':
          return {
            ...state,
            pushWorks: [...state.pushWorks, ...action.filePacks]
          };
        case '@FILESYS/REMOVE_FROM_PUSH_WORK':
          return {
            ...state,
            pushWorks: state.pushWorks.filter( work => {return work.path !== action.path}),
          };
        case '@FILESYS/PUSH_WORKER_GET_JOB':
          return {
            ...state,
            pushWorker: {
              ...state.pushWorks[action.workIdx],
              status:'working',
              offset: 0,
            },
          };
        case '@FILESYS/PUSH_PROGRESS':
          return {
            ...state,
            pushWorker: {...state.pushWorker, status: action.status, offset: action.offset}
          };
        
        
        // case '@FILESYS/PULL_WORKER_SET':
        //   return {
        //     ...state,
        //     pullWorker: {...state.pullWorker, status: action.status}
        //   };
        // case '@FILESYS/PULL_WORK_SET':
        //   return {
        //     ...state,
        //     pullWorks: state.pullWorks.map(p => {
        //       if(p.path === action.path){
        //         return {...p, status: action.status};
        //       }
        //       else{
        //         return p;
        //       }
        //     })
        //   };
        // case '@FILESYS/ADD_TO_PULL_WORK':
        //   return {
        //     ...state,
        //     pullWorks: [...state.pullWorks, {path:action.path, file64: '', status: action.status}]
        //   }; 
        // case '@FILESYS/REMOVE_FROM_PULL_WORK':
        //   return {
        //     ...state,
        //     pullWorks: state.pullWorks.filter( work => {return work.path !== action.path}),
        //   };
        // case '@FILESYS/PULL_WORK_START':
        //   return {
        //     ...state,
        //     pullWorker: {
        //       status: 'working',
        //       ...state.pullWorks[state.pullWorks.length - 1]
        //     },
        //     pullWorks: state.pullWorks.slice(0, state.pullWorks.length - 1)
        //   };    
        // case '@FILESYS/PULL_PROGRESS':
        //   return {
        //     ...state,
        //     file64: state.file64 + action.data
        //   };
        
          default:
          return state;
    }
};