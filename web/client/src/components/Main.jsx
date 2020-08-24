import React from 'react';
import PropTypes from 'prop-types';
import {connect} from 'react-redux';

import {Button, ListGroup, ListGroupItem,
    Collapse,
    Navbar,
    NavbarToggler,
    NavbarBrand,
    Nav,
    NavItem,
    NavLink,
    Input,
    Container,
} from 'reactstrap';

import FileSelector from 'components/FileSelector.jsx';
import FileSys from 'components/FileSys.jsx';

import {test as testFromAct, look as lookFromAct} from 'states/main-actions.js';

class Main extends React.Component {
    static propTypes = {
        store: PropTypes.object,
        dispatch: PropTypes.func,
    };

    constructor(props){
        super(props);
        this.test = this.test.bind(this);
        this.look = this.look.bind(this);
    }
    render(){
        return (
            <div>
                <Navbar color='faded' light>
                    <NavbarBrand className='text-danger'>Airmouse</NavbarBrand>
                </Navbar>

                <FileSys/>

                <Button onClick={this.look}>fs.html</Button>
                <div>
                    {this.props.document ? this.props.document.querySelector('#test').innerHTML : ''}
                    {typeof(this.props.document)}
                </div>
                <Button onClick={this.test}>post test</Button>
                <p>{this.props.status}</p>
                <FileSelector/>
                <div id='downloaders'>
                </div>
            </div>
        );
    }
    look(){
        this.props.dispatch(lookFromAct());
    }
    test(){
        this.props.dispatch(testFromAct());
    }
};

export default connect(state => ({
    ...state.main
}))(Main);