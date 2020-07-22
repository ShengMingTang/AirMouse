import React from 'react';
import PropTypes from 'prop-types';
import {connect} from 'react-redux';
import {Button, ButtonGroup, ListGroup, ListGroupItem, Spinner, ProgressBar} from 'reactstrap';

import {onChange, onDownload} from '../states/fileSys-actions';

const indent = 20;
/*
{
    worker: -1, // worker index -1|0 for now
    label: '', // filename or directory name
    parent: ''|null, // null for root, full path for any directory or file
    children: [{}...]|null // null for leaf node, [] for directory
    url(if cached)
}
*/

class Node extends React.Component{
    static propTypes = {
        store: PropTypes.object,
        dispatch: PropTypes.func,
    };

    constructor(props){
        super(props);
        this.handleClick = this.handleClick.bind(this);
        
        this.state = {
            expanded: false,
            file: '',
            reader: null,
        }
    }

    render(){
        const {status, label, parent, children} = this.props;
        let leaves = null;
        const depth = ("depth" in this.props) ? this.props.depth : 0;
        const style = {
            "position": 'relative',
            'left': depth * indent,
        }
        const addBtn = (
            <ListGroupItem className="list-group-item-dark" style={{"position": 'relative','left': (depth+1) * indent}} key="add file">
                <ButtonGroup size='sm'>
                    <Button className='btn-success' key="add file">
                        <label style={{'margin': 0}}>
                            <input type="file" multiple onChange={(e) => {this.handleFileChange(e.target.files)}} display="none"></input>
                            Add file(s)
                        </label>
                    </Button>
                    <Button className='btn-warning' key="add folder">Add folder</Button>
                </ButtonGroup>
            </ListGroupItem>
        )

        if(children){
            leaves = children.map((p, idx) => {
                return (<Node {...p} depth={depth+1} store={this.props.store} dispatch={this.props.dispatch} key={idx.toString()}/>);
            });
        }
        return (
            <ListGroup size="sm">
                <ListGroupItem className="list-group-item-dark" style={style} key={label}>
                    <Button className="btn-info" onClick={this.handleClick} size="sm">{label}</Button>{' '}
                </ListGroupItem>
                {this.state.expanded && children && leaves}
                {children && addBtn}
            </ListGroup>
        );
    }

    handleClick(){
        if(this.props.children){ // directory, toggle expanded now
            this.setState({expanded: !(this.state.expanded)});
        }
        else{ // leaf, add to pull work
            const parent = this.props.parent ? this.props.parent : ''
            this.props.dispatch(onDownload(parent + this.props.label, this.props.url));
        }
    }

    handleFileChange(files){
        const parent = this.props.parent ? this.props.parent : ''
        this.props.dispatch(onChange(files, parent + this.props.label));
    }
}

export default connect()(Node);