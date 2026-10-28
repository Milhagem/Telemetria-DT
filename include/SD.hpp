#pragma once

void iniciarSD();
/*É a função q vai inicializar o SD, ver se conectou no ESP, criar/abrir um arquivo e criar a queue SD */

void criarTaskSD();
/*Ela vai criar a TaskSD do FreeRTOS, a estrutura da TaskSD deve: esperar uma MeasurementSample chegar na queueSD,
retirar a amostra da queue, colocar o conjunto de dados no buffer de gravação 
e quando o buffer estiver cheio, gravar esse bloco no microSD e limpar/reutilizar o buffer*/