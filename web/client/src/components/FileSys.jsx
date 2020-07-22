import React from 'react';
import PropTypes from 'prop-types';
import {connect} from 'react-redux';
import {Button, ListGroup, ListGroupItem, ButtonGroup, Spinner} from 'reactstrap';

// import Tree from '@naisutech/react-tree';
import Node from 'components/Node.jsx';
import {onSelectNode as onSelectNodeFromApi} from 'api/common.js';

import {
    removeFromPushWork,
    removeFromPullWork,

    onCancelPush,
    onCancelPull,
} from 'states/fileSys-actions.js';

class FileSys extends React.Component{
    static propTypes = {
        store: PropTypes.object,
        dispatch: PropTypes.func,
    };

    constructor(props){
        super(props);
        this.handleCancelPush = this.handleCancelPush.bind(this);
        this.handleCancelPull = this.handleCancelPull.bind(this);

        this.handleRemovePushWork = this.handleRemovePushWork.bind(this);
        this.handleRemovePullWork = this.handleRemovePullWork.bind(this);
        this.pull = this.pull.bind(this);
    }

    render(){
        const genPushWorkItem = (p, idx) => {
            const {path, file64, status} = p;
            return (
                <ListGroupItem className="list-group-item-dark" size="sm" key={idx}>
                    <Spinner animation="border" role="status" size="sm" style={{visibility: status === 'waiting' ? '': 'hidden'}}/>
                    {' '}
                    <ButtonGroup size="sm">
                        <Button className="btn-info" size="sm">{p.path}</Button>{' '}
                        {/* { (status === 'loading' || status === 'waiting')
                            && <Button className="btn-warning">Pause</Button>} */}
                        { (status === 'waiting')
                            && <Button className="btn-danger" onClick={() => {this.handleCancelPush(path)}}>Cancel</Button>}
                        { (status === 'canceled')
                            && <Button className="btn-success" onClick={() => {this.handleRemovePushWork(path)}}>Canceled</Button>}
                        {status === 'done' && <Button className="btn-success" onClick={() => {this.handleRemovePushWork(path)}}>Done</Button>}

                    </ButtonGroup>
                </ListGroupItem>
            )
        };
        const genPullWorkItem = (p, idx) => {
            const {path, file64, status} = p;
            return (
                <ListGroupItem className="list-group-item-dark" size="sm" key={idx}>
                    <Spinner animation="border" role="status" size="sm" style={{visibility: status === 'waiting' ? '': 'hidden'}}/>
                    {' '}
                    <ButtonGroup size="sm">
                        <Button className="btn-info" size ="sm" key="1">{p.path}</Button>{' '}
                        {/* { (status === 'loading' || status === 'waiting')
                            && <Button className="btn-warning" key="2">Pause</Button>} */}
                        { (status === 'waiting')
                            && <Button className="btn-danger" key="3" onClick={() => {this.handleCancelPull(path)}}>Cancel</Button>}
                        { (status === 'canceled')
                            && <Button className="btn-success" key="4" onClick={() => {this.handleRemovePullWork(path)}}>Canceled</Button>}
                        {status === 'done' && <Button className="btn-success" key="4" onClick={() => {this.handleRemovePullWork(path)}}>Done</Button>}

                    </ButtonGroup>
                </ListGroupItem>
            )
        };
        const pushWorks = this.props.pushWorks.map(genPushWorkItem);
        const pullWorks = this.props.pullWorks.map(genPullWorkItem);
        
        return (
        <div style={{ width: '50%', flexGrow: 1, 'marginLeft': 'auto', 'marginRight': 'auto'}}>
            <div>
                {/* <Tree nodes={this.props.tree} grow onSelect={onSelectNodeFromApi}/> */}
                <Node {...this.props.tree[0]}/>
            </div>

            <ListGroup size="sm" style={{'marginTop':50}}>
                Push Works
                {pushWorks}
            </ListGroup>
            <ListGroup size="sm" style={{'marginTop':50}}>
                Pull Works
                {pullWorks}
            </ListGroup>
            <div>
                <Button onClick={this.pull}>pull</Button>
            </div>
        </div>
        );
    }
    
    handleCancelPush(path){
        this.props.dispatch(onCancelPush(path));
    }
    handleCancelPull(path){
        this.props.dispatch(onCancelPull(path));
    }

    handleRemovePushWork(path){
        this.props.dispatch(removeFromPushWork(path));
    }
    handleRemovePullWork(path){
        this.props.dispatch(removeFromPullWork(path));
    }
    pull(){
        this.props.dispatch()
    }
}

export default connect(state => ({
    ...state.fileSys
}))(FileSys);