<?php

class IndexController extends Lux\Controller{
    public function __construct(){
        
    }

    public function index() {
        $this->render("home",["name"=>"World","description"=>"lux view test"]);
    }

    public function post1() {
        // $name = $this->post("name");
        $json = $this->input();

        $this->html("sbssbssbs name = ");
    }
    
}

$app = new Lux\App("0.0.0.0",9002, 4, __DIR__ . '');
$app->view()->setLayout("layout");
$router = $app->router();

$router->get("/abc/:id", 'IndexController@index');
$router->post("/post", 'IndexController@post1');
$app->start();
