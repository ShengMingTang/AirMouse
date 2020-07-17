const initMainState = {
    status: ''
};

export function main(state = initMainState, action){
    switch(action.type){
        case '@MAIN/STATUS':
            return {
                ...state,
                status: action.status
            };
        default:
            return state;
    }
}