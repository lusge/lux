<?php

$srv = new Lux\HttpServer("0.0.0.0",9002, 4);

$router = $srv->router();


// $srv->use(function($request, $response, $next){
    
//     $next();
// });


// $srv->get("/", function($request, $response, $params){
//     $hm = <<<HTML
// <!DOCTYPE html><html><body>
//     <script>
//     var ws = new WebSocket('ws://' + location.host + '/wstest');
//     ws.onopen    = () => ws.send('hello');
//     ws.onmessage = e => document.body.innerHTML += e.data + '<br>';
//     </script>
// </body></html>
// HTML;
//     $response->html($hm);
// });

$router->get("/abc/:id", function($request, $response){
    $response->html("<h1>Hello World<h1>");
    // $response->text("<h1>Hello World<h1>");
    // $response->json(["a"]);
});

$router->group("/v1", function($r){
        $r->get("/abc/:id", function($request, $response){
        $response->html("<h1>Hello World Lucas<h1>" . $request->params["id"] . "<br> ".$request->query["id"]);
        // $response->text("<h1>Hello World<h1>");
        // $response->json(["a"]);
    }); 
})->middleware(function($request, $response, $next) {
    echo "middleware 1 \n";
    $next();
})->middleware(function($request, $response, $next) {
    echo "middleware 2 \n";
    $next();
});

// $srv->notFound(function(){
//     echo "NOT FOUND PHP \n";
// });

// $srv->ws("/wstest",[
//     "open"=>function($ws){
//         echo "This is open function\n";
//     },
//     "message"=>function($webs){
//         $a =  $webs->message; 
//         echo $a . "\n";
// var_dump($webs);
//         $webs->write("HELLO WORLD");
//     },
//     "close"=>function($uuid){}
// ]);

$srv->run();
var_dump($srv);

