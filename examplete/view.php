<?php

$view = new Lux\View("./");
$view->setLayout("layout");
echo $view->render("home",["name"=>"World","description"=>"lux view test"]);