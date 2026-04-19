<?php

$r = new Lux\Router();
$r->get("/health", function(){})->name("health");

$r->group("/api", function($router){
    $router->post("/login", 'App\Controllers/AuthController@login');
    $router->group('/v1', function($r) {
        $r->put("/users/:id", 'App\Controller\UserController@update');
        $r->delete("/users/:id", 'App\ControllerUsercontroller@delete');
    })->middleware(function(){

    });
});

$r->group("/api2", function($router){
    $router->post("/login", 'App\Controllers/AuthController@login');
    $router->group('/v2', function($r) {
        $r->put("/users/:id", 'App\Controller\UserController@update');
        $r->delete("/users/:id", 'App\ControllerUsercontroller@delete');
    })->middleware(function(){

    });
});

$r->ws('/ws/chat', [
    'open'=>function(){},
    "message"=>function(){},
    "close"=>function(){}
]);

$r->notFound(function(){});

$r->print();

var_dump($r);