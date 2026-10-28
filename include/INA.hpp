#pragma once

void iniciarINA();
/* esse função vai ser todos os parâmetros que são necessários para configurar o INA, como:
corrente máxima, valor do shunt, tempo de conversão, media de amostras(provavelmente vai ser 1 apenas), 
ADCrange e configuração do alert, basicamenente oq foi feito no código de teste */

void criarTaskINA();
/*usar a função xTaskCreate() para criar uma task, deve ter prioriade 3(maior prioridade)
quando for fazer essa função, olhar a estrutura dessa função padão. dentro dela, vai existir a taskINA, 
que ela sim vai fazer a aquisição dos dados e vai deixar pronto para enviar para a queue do SD*/