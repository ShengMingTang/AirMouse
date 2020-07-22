import {createDownloader as createDownloaderFromApi} from 'api/common.js';

const initFileSysState = {
    downloaders: [
      // createDownloaderFromApi('YWFhYWFh', 'a.txt'),
      // createDownloaderFromApi('YmJiYmJi', 'b.txt'),
      // createDownloaderFromApi('Y2NjY2Nj', 'c.txt')
    ],

    pushWorker: {
      status: 'idle', // 'idle'|'working'
      path: '',
      file64: '',
      seq: 0,
      offset: 0,
    },
    pushWorks: [
      {path: '/temp/aa.txt', file64: '', status: 'loading'},
      {path: 'cc.txt', file64: '', status: 'waiting'}
    ],

    pullWorker: {
      status: 'idle', // 'idle'|'working'
      path: '',
      file64: '',
    },
    pullWorks: [ // {path}
      {path: '/temp/a.txt', file64: '', status: 'loading'},
      {path: '/temp/b.txt', file64: '', status: 'waiting'},
      {path: '/temp/c.txt', file64: '', status: 'canceled'},
      {path: '/temp/d.txt', file64: '', status: 'done'},
    ],
    tree: [
      {
        pushWorker: -1,
        label: '/',
        parent: null,
        children: [
          {
            pushWorker: -1,
            label: 'temp/',
            parent: '/',
            children: [
              {
                pushWorker: -1,
                label: 'a.txt',
                parent: '/temp/',
              },
            ]
          },
          {
            pushWorker: -1,
            label: 'small.txt',
            parent: '/',
          }
        ]
      }
    ],    
};

export function fileSys(state = initFileSysState, action){
    switch(action.type){
        case '@FILESYS/PUSH_WORKER_SET':
          return {
            ...state,
            pushWorker: {...state.pushWorker, status: action.status}
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
        case '@FILESYS/CANCEL_PUSH':
          return {
            ...state,
            pushWorks: state.pushWorks.map(p => {
              if(p.path === action.path){
                return {...p, status: 'canceled'};
              }
              return p;
            })
          }
        case '@FILESYS/PUSH_WORK_START':
          return {
            ...state,
            pushWorker: {
              status:'working',
              seq: 0, offset: 0,
              ...state.pushWorks[state.pushWorks.length - 1]
            },
            pushWorks: state.pushWorks.slice(0, state.pushWorks.length - 1)
          };

        case '@FILESYS/PUSH_PROGRESS':
          return {
            ...state,
            pullWorker: {...state.pushWorker, status: action.status, seq: action.seq, offset: action.offset}
          };
        
        case '@FILESYS/PULL_WORKER_SET':
          return {
            ...state,
            pullWorker: {...state.pullWorker, status: action.status}
          };
        case '@FILESYS/ADD_TO_PULL_WORK':
          return {
            ...state,
            pullWorks: [...state.pullWorks, {path:action.path, file64: '', status: action.status}]
          }; 
        case '@FILESYS/REMOVE_FROM_PULL_WORK':
          return {
            ...state,
            pullWorks: state.pullWorks.filter( work => {return work.path !== action.path}),
          }
        case '@FILESYS/CANCEL_PULL':
          return {
            ...state,
            pullWorks: state.pullWorks.map(p => {
              if(p.path === action.path){
                return {...p, status: 'canceled'};
              }
              return p;
            })
          }
        case '@FILESYS/PULL_WORK_START':
          return {
            ...state,
            pullWorker: {
              status: 'working',
              ...state.pullWorks[state.pullWorks.length - 1]
            },
            pullWorks: state.pullWorks.slice(0, state.pullWorks.length - 1)
          };
        
        
        case '@FILESYS/PULL_START':
          return {
            ...state,
            file64: action.file64,
            filename: action.filename
          };
        case '@FILESYS/PULL_PROGRESS':
          return {
            ...state,
            file64: state.file64 + action.data
          };
        case '@FILESYS/PULL_END':
          return {
            ...state,
            status: action.status,
            downloaders: [...state.downloaders, action.downloader]
          }
        default:
          return state;
    }
};