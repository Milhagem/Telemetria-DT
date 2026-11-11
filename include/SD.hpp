#pragma once

#include <Arduino.h>
bool iniciarSD();
/*É a função q vai inicializar o SD, ver se conectou no ESP, criar/abrir um arquivo e criar a queue SD */

void criarTaskSD();
/*Ela vai criar a TaskSD do FreeRTOS, a estrutura da TaskSD deve: esperar uma Amostra chegar na queueSD,
retirar a amostra da queue, colocar o conjunto de dados no buffer de gravação 
e quando o buffer estiver cheio, gravar esse bloco no microSD e limpar/reutilizar o buffer*/

void criarNovaSessaoSD();

/*Também é bom já fazer a serialização desses dados, para q já fiquem organizados, n é tão necessário fazer agr, mas n é ruim*/