<?php 

$host = 'localhost:8889'; // адрес сервера 
$database = 'onlinePot'; // имя базы данных
$user = 'root'; // имя пользователя
$passwordDB = 'root'; // пароль


$pot_id = $_REQUEST['pot_id'];
$soil_moisture = $_REQUEST['soil_moisture'];
$uv_index = $_REQUEST['uv_index'];
$water_level = $_REQUEST['water_level'];
$ph_value = $_REQUEST['ph_value'];
$distance = $_REQUEST['distance'];
$lux = $_REQUEST['lux'];
$temperature = $_REQUEST['temperature'];
$pressure = $_REQUEST['pressure'];
$humidity = $_REQUEST['humidity'];
$releyValue = $_REQUEST['releyValue'];
$email = $_REQUEST['email'];
$password = $_REQUEST['password'];
$name = $_REQUEST['name'];
$pot = $_REQUEST['pot'];
$led = $_REQUEST['led'];
$session = $_REQUEST['session']; 
$method = $_REQUEST['method'];
$preferencePlants=$_REQUEST['preferencePlants'];
$altitude = $_REQUEST['altitude'];
$light_relay = $_REQUEST['light_relay']; 
$pomp_relay = $_REQUEST['pomp_relay'];

// Пробуем подключиться к БД 
$dbConnect = new mysqli($host, $user, $passwordDB, $database);

// метод регистрации 
if ($method == "reg") {
    if (isset($name, $email, $password, $pot)) {

    // Если подключение не успещное , возвращаем ошибку клиену
        if (mysqli_connect_errno()) {
            $resultError = array('error' => "Не удалось подключиться:",mysqli_connect_error());
            echo json_encode($resultError);
            exit();
        }

    // проверяем существует ли пользователь с таким же email 
        $num_rows = mysqli_num_rows(mysqli_query($dbConnect, "SELECT id FROM clients WHERE email = '$email'")); 
        if($num_rows !=0){
            $resultError = array('error' => "Пользователь с таким адрессом электронной почты уже существует");
            echo json_encode($resultError);
            exit();
        } 

        // шифруем пароль SHA1
        $pwd  = sha1($password + $email);

        // добавляем новго пользователя 
        if (mysqli_query($dbConnect, "INSERT INTO clients SET name = '$name', email = '$email', session = '$pwd'")) {
            // получаем данные из БД
            $clientData = mysqli_query($dbConnect, "SELECT id FROM clients WHERE email = '$email'");
            // достаем ID 
            while ($row = mysqli_fetch_assoc($clientData)) {
                $clientID = $row["id"];
                $session = $row["session"];
            }

            // добавляем ID горшка 
            mysqli_query($dbConnect, "INSERT INTO pots SET client_id = '$clientID', pot_id = '$pot'");
            $result = array('status' => "2", 'session' => $pwd, 'message' => "Регистрация прошла успешна");
            echo json_encode($result); 
            exit();
        } else {
            $result = array('error' => "Упс, что-то пошло не так...", 'mysqli_connect_error' => mysqli_connect_error());
            echo json_encode($result); 
        }
    } else {
        $resultError = array('error' => "Заполните все обязательные поля" );
        echo json_encode($resultError);
    }
} 

if($method == "auth") {
    if (isset($email, $password)) {
        // проверяем существует ли пользователь
        $checkEmail = mysqli_num_rows(mysqli_query($dbConnect, "SELECT * FROM clients WHERE email = '$email'"));
        if($checkEmail == 1){
            $session = sha1($password + $email);
            // проверяем существования хеша пароля  
            $checkClient = mysqli_num_rows(mysqli_query($dbConnect, "SELECT * FROM clients WHERE session = '$session' AND email = '$email'"));
            if ($checkClient == 1){
                $result = array('session' => $session, 'message' => "Добро пожаловать");
                echo json_encode($result);
                exit();
            } else {
                $result = array('error' => "Упс, что-то пошло не так...", 'mysqli_connect_error' => mysqli_connect_error());
                echo json_encode($result); 
            }
        } else {
            $resultError = array('error' => "Неправельный логин или пароль");
            echo json_encode($resultError);
        }
    } else {
        $resultError = array('error' => "Заполните все обязательные поля" );
        echo json_encode($resultError);
    }
}

if ($method == "getConfig"){
    $clientData = mysqli_query($dbConnect, "SELECT * FROM clients WHERE session = '$session'");
    $num_rows = mysqli_num_rows($clientData);
    if ($num_rows != 0 ){
        // Достаем id клиента  - таблица "clients"
        while ($row = mysqli_fetch_assoc($clientData)) {
         $clientID = $row['id'];
     }
        //используя id клиента из таблицы "clients" достаем горшки принадлежащие этому клиенту
     $existsPot = mysqli_query($dbConnect, "SELECT * FROM pots WHERE client_id = '$clientID'");
     while ($row = mysqli_fetch_assoc($existsPot)) {
         $checkPot = $row['pot_id'];
     }
     if ($checkPot == $pot){
        $configurationGrowth = mysqli_query($dbConnect, "SELECT * FROM configurationGrowth WHERE id = '$preferencePlants'");
        while ($row = mysqli_fetch_array($configurationGrowth)) {
            $settingSoilMoisture = $row['soil_moisture']; 
            $settingTimeOn = $row['time_on'];
            $settingTimeOff = $row['time_off'];
        }

        // Отправка текущих показаний NodeMCU клиента 
        $config = mysqli_query($dbConnect, "SELECT * FROM pots WHERE pot_id = '$pot'");
        while ($row = mysqli_fetch_array($config)) {
            $led = $row['led'];
            $clientID = $row['id'];
            $pot = $row['pot_id'];
            $soil_moisture = $row['soil_moisture'];
            $uv_index = $row['uv_index'];
            $water_level = $row['water_level'];
            $ph_value = $row['ph_value'];
            $distance = $row['distance'];
            $lux = $row['lux'];
            $temperature = $row['temperature'];
            $pressure = $row['pressure'];
            $humidity = $row['humidity'];
            $releyValue = $row['releyValue'];
        }
        $result = array('clientID' => $clientID, 'pot' => $pot,  $object = array('tete' => $led, ),'led' => $led, 'soil_moisture' => $soil_moisture, 'uv_index' => $uv_index, 'water_level' => $water_level, 'ph_value' => $ph_value, 'distance' => $distance, 'lux' => $lux, 'temperature' => $temperature, 'pressure' => $pressure, 'humidity' => $humidity, 'releyValue' => $releyValue, 'settingSoilMoisture' => $settingSoilMoisture, 'settingTimeOn' => $settingTimeOn, 'settingTimeOff' => $settingTimeOff);
        echo json_encode($result);
    } else {
        $resultError = array('error' => "Доступ ограничен");
        echo json_encode($resultError);
        } 
    } else {
    $resultError = array('error' => "Сессия не найдена");
    echo json_encode($resultError);
    }
}

if ($method == "updateData"){

    if(isset($pot_id)){
        $updateConfig = mysqli_query($dbConnect, "UPDATE pots SET soil_moisture = '$soil_moisture', uv_index = '$uv_index', water_level = '$water_level', ph_value = '$ph_value', temperature = '$temperature', pressure = '$pressure', humidity = '$humidity', altitude = '$altitude', light_relay = '$light_relay', pomp_relay ='$pomp_relay' WHERE pot_id = '$pot_id'");
        
        $selectPreferencePlants = mysqli_query($dbConnect, "SELECT * FROM pots WHERE pot_id = '$pot_id'");
        while ($row = mysqli_fetch_array($selectPreferencePlants)) {
            $test = $row['preference_plants']; 
        }

        $configurationGrowth = mysqli_query($dbConnect, "SELECT * FROM configurationGrowth WHERE id = '$test'");
         while ($row = mysqli_fetch_array($configurationGrowth)) {
            $settingSoilMoisture = $row['soil_moisture']; 
            $settingTimeOn = $row['time_on'];
            $settingTimeOff = $row['time_off'];
        }
        

        $result = array('settingSoilMoisture' => $settingSoilMoisture, 'settingTimeOn' => $settingTimeOn,'settingTimeOff' => $settingTimeOff);
        echo json_encode($result);
    } else {
        echo json_encode('Check variables pot_id and preferencePlants');
    }
}
// http://localhost:8888/?method=reg&email=pupok@test.ru&password=test&name=test&pot=4
// http://localhost:8888/?method=auth&email=pupok@test.ru&password=test
// http://localhost:8888/?method=getConfig&pot=4&preferencePlants=1&session=b6589fc6ab0dc82cf12099d1c2d40ab994e8410c
// http://192.168.1.4:8888/?method=updateData&pot_id=4&soil_moisture=5&temperature=25&pressure=973&altitude=137&humidity=26&water_level=85&light_relay=On&pomp_relay=Off
?>