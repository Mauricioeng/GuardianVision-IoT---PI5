// ID AppSheet
const SHEET_ID = '15XXXXXXXXXXXXXXXXxyL4UWJo1IHthkXXAXX';

function doGet() {
  return HtmlService.createHtmlOutputFromFile('Index')
    .setTitle('GuardianVision | Saúde ODS 3')
    .addMetaTag('viewport', 'width=device-width, initial-scale=1');
}

// Essa função será chamada pelo React para salvar os dados na planilha!
function registrarUsuario(dados) {
  try {
    const planilha = SpreadsheetApp.openById(SHEET_ID);
    let aba = planilha.getSheetByName('Usuarios');
    
    // Se a aba "Usuarios" não existir na sua planilha, o robô cria uma automaticamente!
    if (!aba) {
      aba = planilha.insertSheet('Usuarios');
      aba.appendRow(['Data do Cadastro', 'Nome', 'E-mail', 'Senha']); 
    }
    
    // Salva a nova linha na planilha
    aba.appendRow([new Date(), dados.nome, dados.email, dados.senha]);
    
    return { status: 'success', message: 'Cadastro realizado com sucesso!', user: dados };
  } catch (erro) {
    return { status: 'error', message: 'Erro ao salvar na planilha: ' + erro.toString() };
  }
}
