; Inno Setup 6.6+ script. Build: cmake --install to a stage folder, then
;   iscc /DAppVersion=1.6.0 /DStageDir=<stage> installer\OBS-VST-Ex-Audio-Filter.iss
; Installs two OBS modules: VST Ex (OBS-VST-Ex-Audio-Filter) and VST Enigmax Loader (vst-enigmax-loader).
; The custom setup's choices go to install-features.ini, which VST Ex reads once at its next load.
; Uninstall asks what to keep; a backup it makes has the same layout as Tools > Debug mode > Backup.

#ifndef AppVersion
  #define AppVersion "1.6.0"
#endif
#ifndef StageDir
  #define StageDir "..\build_x64\stage"
#endif
#define MainName "OBS-VST-Ex-Audio-Filter"
#define LoaderName "vst-enigmax-loader"
#define FeatureIni "{userappdata}\obs-studio\plugin_config\" + MainName + "\install-features.ini"

[Setup]
AppId={{6B0E7C51-3F2A-4D8B-9E55-1C7A0F4B2D93}
AppName=VST Ex Audio Filter for OBS
AppVersion={#AppVersion}
AppPublisher=ChromeBread feat. BlackDream
AppPublisherURL=https://github.com/ChromeBread/VST-Ex-Audio-Filter-for-OBS
; {app} is OBS's plugin folder; each module gets its own subfolder there.
DefaultDirName={commonappdata}\obs-studio\plugins
UsePreviousAppDir=no
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallFilesDir={app}\{#MainName}\uninstall
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile=..\LICENSE
OutputDir=..\build_x64\installer
OutputBaseFilename={#MainName}-{#AppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; Wide enough that the long component descriptions are not cut off.
WizardSizePercent=150,120
WizardResizable=yes
ShowLanguageDialog=auto

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ja"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "ko"; MessagesFile: "compiler:Languages\Korean.isl"

[CustomMessages]
en.TypeFull=Full installation
en.TypeCustom=Custom installation
en.PartCore=VST Ex Audio Filter (required): Filters (VST Ex) in the right-click menu, pitch / pseudo-stereo filters, Tools > #cbOBScripts > VST Ex Audio Filter
en.PartLoader=VST Enigmax Loader: loads VST2 / VST3 plugins (safe scan, plugin browser, editor screenshots, settings export / import)
en.PartChainView=VST Ex Chain View: shows the VST chain of every audio source (devices, filters, latency, levels) with a sound imager. Tools menu and dock button
en.PartMixer=VST Ex Mixer dock: under each Audio Mixer meter, one button per VST filter of that source. Click: ON/OFF, right-click: open the plugin
en.PartSwitcher=VST Ex Switcher dock: ON/OFF set buttons, and CPU usage of OBS with the share used by VST plugins
en.PartWaveline=VST Ex WaveLine dock: waveforms of chosen sources flowing on top of each other; sources that reach the stream are strong, others faint
en.CloseObs=OBS is running. Quit OBS, then click Retry.
en.LoaderFiltersLost=Your scene collections have %1 VST Enigmax Loader filter(s). Without VST Enigmax Loader they disappear from OBS.%n%nTo keep them, first move them to OBS VST 2.x / atkAudio in OBS: Tools > #cbOBScripts > VST Enigmax Loader > Debug mode > Hand over.%n%nStop now to do that?
en.KeepTitle=What should be removed?
en.KeepIntro=Choose what to delete. Anything left unticked stays on this PC.
en.DelThumbs=Plugin thumbnails (low resolution)
en.DelShots=High-resolution screenshots
en.DelSettings=Settings, chain presets and filter history
en.ExportFirst=Export everything before deleting
en.ExportWhere=Folder for the backup (a VST-Ex-backup folder is created inside):
en.OK=OK
en.Strays=These files in the plugin's folders were not made by it (other files, or renamed copies such as .bak / .org). Tick the ones to delete; the rest are kept.
en.DeleteTicked=Delete ticked
en.KeepAll=Keep all
en.Exported=Backup saved to:%n%1
en.StraysFound=%1 file(s) and %2 folder(s) in the plugin's folders were not made by it (other files, or renamed copies such as .bak / .org). Folders that are not ours are never opened or deleted.
en.StraysKeep=Keep them and go on
en.StraysChoose=Choose files to delete...
en.StraysSave=Save the list and stop
en.StraysSaved=The list was saved to:%n%1%n%nThe uninstall was stopped; nothing was removed.
en.SelectAll=Select all
en.SelectNone=Select none
ja.TypeFull=フル インストール
ja.TypeCustom=カスタム インストール
ja.PartCore=VST Ex Audio Filter (必須): 右クリックの フィルタ (VST Ex)、ピッチ / 疑似ステレオ フィルタ、ツール > #cbOBScripts > VST Ex Audio Filter
ja.PartLoader=VST Enigmax Loader: VST2 / VST3 プラグインの読み込み (安全なスキャン、プラグインブラウザ、エディタのスクリーンショット、設定のエクスポート / インポート)
ja.PartChainView=VST Ex Chain View: 全オーディオソースの VST チェーン (デバイス・フィルタ・レイテンシ・レベル) の表示とサウンドイメージャー。ツールメニューとドックのボタン
ja.PartMixer=VST Ex Mixer ドック: オーディオミキサーの各メーターの下に、そのソースの VST フィルタごとのボタン。クリックで ON/OFF、右クリックでプラグインを開く
ja.PartSwitcher=VST Ex Switcher ドック: ON/OFF セットのボタンと、OBS の CPU 使用率のうち VST プラグインが使っている割合の表示
ja.PartWaveline=VST Ex WaveLine ドック: 選んだソースの波形を重ねて流します。配信に載るソースは濃く、載らないソースは薄く表示します
ja.CloseObs=OBS が起動しています。OBS を終了してから「再試行」を押してください。
ja.LoaderFiltersLost=シーンコレクションに VST Enigmax Loader のフィルタが %1 個あります。VST Enigmax Loader がないと OBS から消えます。%n%n残すには、先に OBS の ツール > #cbOBScripts > VST Enigmax Loader > デバッグモード > 引き継ぎ で OBS VST 2.x / atkAudio へ移してください。%n%nいったん中止して移しますか?
ja.KeepTitle=何を削除しますか?
ja.KeepIntro=削除するものを選んでください。チェックしなかったものはこの PC に残ります。
ja.DelThumbs=プラグインのサムネイル (低解像度)
ja.DelShots=高解像度スクリーンショット
ja.DelSettings=設定・チェーンのプリセット・フィルタの履歴
ja.ExportFirst=全てエクスポートしてから削除する
ja.ExportWhere=バックアップの保存先 (中に VST-Ex-backup フォルダを作ります):
ja.OK=OK
ja.Strays=プラグインのフォルダに、プラグインが作ったものではないファイルがあります (別のファイルや、.bak / .org など名前を変えた複製)。削除するものにチェックしてください。それ以外は残します。
ja.DeleteTicked=チェックしたものを削除
ja.KeepAll=すべて残す
ja.Exported=バックアップを保存しました:%n%1
ja.StraysFound=プラグインのフォルダに、プラグインが作ったものではないファイルが %1 個、フォルダが %2 個あります (別のファイルや .bak / .org など名前を変えた複製)。プラグインのものではないフォルダは、中を開くことも削除することもしません。
ja.StraysKeep=残して続ける
ja.StraysChoose=削除するファイルを選ぶ...
ja.StraysSave=一覧を保存して中止
ja.StraysSaved=一覧を保存しました:%n%1%n%nアンインストールは中止しました。何も削除していません。
ja.SelectAll=すべて選択
ja.SelectNone=すべて解除
fr.TypeFull=Installation complète
fr.TypeCustom=Installation personnalisée
fr.PartCore=VST Ex Audio Filter (requis) : Filtres (VST Ex) dans le clic droit, filtres hauteur / pseudo-stéréo, Outils > #cbOBScripts > VST Ex Audio Filter
fr.PartLoader=VST Enigmax Loader : charge les plugins VST2 / VST3 (analyse sûre, navigateur, captures des éditeurs, export / import des réglages)
fr.PartChainView=VST Ex Chain View : la chaîne VST de chaque source audio (périphériques, filtres, latence, niveaux) avec un imageur sonore. Menu Outils et bouton du dock
fr.PartMixer=Dock VST Ex Mixer : sous chaque vumètre du mixeur audio, un bouton par filtre VST de la source. Clic : ON/OFF, clic droit : ouvrir le plugin
fr.PartSwitcher=Dock VST Ex Switcher : boutons des ensembles ON/OFF, et utilisation CPU d'OBS avec la part prise par les plugins VST
fr.PartWaveline=Dock VST Ex WaveLine : les formes d'onde des sources choisies défilent superposées ; celles qui vont au stream sont foncées, les autres pâles
fr.CloseObs=OBS est ouvert. Quittez OBS puis cliquez sur Réessayer.
fr.LoaderFiltersLost=Vos collections de scènes contiennent %1 filtre(s) VST Enigmax Loader. Sans VST Enigmax Loader, ils disparaissent d'OBS.%n%nPour les garder, déplacez-les d'abord vers OBS VST 2.x / atkAudio dans OBS : Outils > #cbOBScripts > VST Enigmax Loader > Mode débogage > Transfert.%n%nArrêter maintenant pour le faire ?
fr.KeepTitle=Que faut-il supprimer ?
fr.KeepIntro=Choisissez ce qui est supprimé. Ce qui n'est pas coché reste sur ce PC.
fr.DelThumbs=Miniatures des plugins (basse résolution)
fr.DelShots=Captures haute résolution
fr.DelSettings=Réglages, préréglages de chaîne et historique des filtres
fr.ExportFirst=Tout exporter avant de supprimer
fr.ExportWhere=Dossier de la sauvegarde (un dossier VST-Ex-backup y est créé) :
fr.OK=OK
fr.Strays=Ces fichiers dans les dossiers du plugin n'ont pas été créés par lui (autres fichiers, ou copies renommées .bak / .org). Cochez ceux à supprimer ; les autres sont gardés.
fr.DeleteTicked=Supprimer les cochés
fr.KeepAll=Tout garder
fr.Exported=Sauvegarde enregistrée dans :%n%1
fr.StraysFound=%1 fichier(s) et %2 dossier(s) dans les dossiers du plugin n'ont pas été créés par lui (autres fichiers, ou copies renommées .bak / .org). Les dossiers qui ne sont pas à lui ne sont jamais ouverts ni supprimés.
fr.StraysKeep=Les garder et continuer
fr.StraysChoose=Choisir les fichiers à supprimer...
fr.StraysSave=Enregistrer la liste et arrêter
fr.StraysSaved=La liste a été enregistrée dans :%n%1%n%nLa désinstallation a été arrêtée ; rien n'a été supprimé.
fr.SelectAll=Tout cocher
fr.SelectNone=Tout décocher
ko.TypeFull=전체 설치
ko.TypeCustom=사용자 지정 설치
ko.PartCore=VST Ex Audio Filter (필수): 우클릭 메뉴의 필터 (VST Ex), 피치 / 의사 스테레오 필터, 도구 > #cbOBScripts > VST Ex Audio Filter
ko.PartLoader=VST Enigmax Loader: VST2 / VST3 플러그인 불러오기 (안전한 스캔, 플러그인 브라우저, 에디터 스크린샷, 설정 내보내기 / 가져오기)
ko.PartChainView=VST Ex Chain View: 모든 오디오 소스의 VST 체인 (장치, 필터, 지연, 레벨) 표시와 사운드 이미저. 도구 메뉴와 도크 버튼
ko.PartMixer=VST Ex Mixer 도크: 오디오 믹서의 각 미터 아래에 그 소스의 VST 필터마다 버튼. 클릭: ON/OFF, 우클릭: 플러그인 열기
ko.PartSwitcher=VST Ex Switcher 도크: ON/OFF 세트 버튼과, OBS CPU 사용률 중 VST 플러그인이 쓰는 비율 표시
ko.PartWaveline=VST Ex WaveLine 도크: 선택한 소스의 파형을 겹쳐서 흘려 보여 줍니다. 방송에 나가는 소스는 진하게, 나머지는 옅게 표시합니다
ko.CloseObs=OBS가 실행 중입니다. OBS를 종료한 뒤 다시 시도를 누르세요.
ko.LoaderFiltersLost=장면 모음에 VST Enigmax Loader 필터가 %1개 있습니다. VST Enigmax Loader가 없으면 OBS에서 사라집니다.%n%n유지하려면 먼저 OBS의 도구 > #cbOBScripts > VST Enigmax Loader > 디버그 모드 > 인계 에서 OBS VST 2.x / atkAudio로 옮기세요.%n%n지금 중단하고 옮길까요?
ko.KeepTitle=무엇을 삭제할까요?
ko.KeepIntro=삭제할 항목을 고르세요. 체크하지 않은 항목은 이 PC에 남습니다.
ko.DelThumbs=플러그인 썸네일 (저해상도)
ko.DelShots=고해상도 스크린샷
ko.DelSettings=설정, 체인 프리셋, 필터 기록
ko.ExportFirst=모두 내보낸 뒤 삭제
ko.ExportWhere=백업 저장 폴더 (안에 VST-Ex-backup 폴더를 만듭니다):
ko.OK=확인
ko.Strays=플러그인 폴더에 플러그인이 만들지 않은 파일이 있습니다 (다른 파일이나 .bak / .org 같은 이름을 바꾼 복사본). 삭제할 파일을 체크하세요. 나머지는 남겨 둡니다.
ko.DeleteTicked=체크한 파일 삭제
ko.KeepAll=모두 유지
ko.Exported=백업을 저장했습니다:%n%1
ko.StraysFound=플러그인 폴더에 플러그인이 만들지 않은 파일 %1개와 폴더 %2개가 있습니다 (다른 파일이나 .bak / .org 같은 이름을 바꾼 복사본). 플러그인 것이 아닌 폴더는 열거나 삭제하지 않습니다.
ko.StraysKeep=남기고 계속
ko.StraysChoose=삭제할 파일 고르기...
ko.StraysSave=목록 저장 후 중단
ko.StraysSaved=목록을 저장했습니다:%n%1%n%n제거를 중단했습니다. 아무것도 삭제하지 않았습니다.
ko.SelectAll=모두 선택
ko.SelectNone=모두 해제

[Types]
Name: "full"; Description: "{cm:TypeFull}"
Name: "custom"; Description: "{cm:TypeCustom}"; Flags: iscustom

[Components]
Name: "core"; Description: "{cm:PartCore}"; Types: full custom; Flags: fixed
Name: "loader"; Description: "{cm:PartLoader}"; Types: full
Name: "chainview"; Description: "{cm:PartChainView}"; Types: full
Name: "mixer"; Description: "{cm:PartMixer}"; Types: full
Name: "switcher"; Description: "{cm:PartSwitcher}"; Types: full
Name: "waveline"; Description: "{cm:PartWaveline}"; Types: full

[InstallDelete]
; 1.0.0 had the scanner inside VST Ex's folder; it now belongs to VST Enigmax Loader.
Type: files; Name: "{app}\{#MainName}\bin\64bit\vst-ex-scanner.exe"
; Unticking the loader on a reinstall removes it (the warning comes on the components page).
Type: filesandordirs; Name: "{app}\{#LoaderName}"; Components: not loader

[Files]
Source: "{#StageDir}\{#MainName}\*"; DestDir: "{app}\{#MainName}"; Excludes: "*.pdb"; Flags: recursesubdirs ignoreversion; Components: core
Source: "{#StageDir}\{#LoaderName}\*"; DestDir: "{app}\{#LoaderName}"; Excludes: "*.pdb"; Flags: recursesubdirs ignoreversion; Components: loader

[Dirs]
Name: "{userappdata}\obs-studio\plugin_config\{#MainName}"

[INI]
Filename: "{#FeatureIni}"; Section: "features"; Key: "featureChainView"; String: "true"; Components: chainview
Filename: "{#FeatureIni}"; Section: "features"; Key: "featureChainView"; String: "false"; Components: not chainview
Filename: "{#FeatureIni}"; Section: "features"; Key: "featureMixer"; String: "true"; Components: mixer
Filename: "{#FeatureIni}"; Section: "features"; Key: "featureMixer"; String: "false"; Components: not mixer
Filename: "{#FeatureIni}"; Section: "features"; Key: "featureSwitcher"; String: "true"; Components: switcher
Filename: "{#FeatureIni}"; Section: "features"; Key: "featureSwitcher"; String: "false"; Components: not switcher
Filename: "{#FeatureIni}"; Section: "features"; Key: "featureWaveline"; String: "true"; Components: waveline
Filename: "{#FeatureIni}"; Section: "features"; Key: "featureWaveline"; String: "false"; Components: not waveline

[Code]
var
  DelThumbs, DelShots, DelSettings, ExportFirst: Boolean;
  ExportDir: String;

{ ---------- OBS running ---------- }

function ObsRunning: Boolean;
var
  Locator, Service, Found: Variant;
begin
  Result := False;
  try
    Locator := CreateOleObject('WbemScripting.SWbemLocator');
    Service := Locator.ConnectServer('.', 'root\CIMV2');
    Found := Service.ExecQuery('SELECT ProcessId FROM Win32_Process WHERE Name="obs64.exe" OR Name="obs32.exe"');
    Result := Found.Count > 0;
  except
  end;
end;

function WaitForObsClosed: Boolean;
begin
  Result := True;
  while ObsRunning do
    if MsgBox(CustomMessage('CloseObs'), mbError, MB_RETRYCANCEL) <> IDRETRY then
    begin
      Result := False;
      Exit;
    end;
end;

{ ---------- paths ---------- }

function ConfigDir(Name: String): String;
begin
  Result := ExpandConstant('{userappdata}\obs-studio\plugin_config\') + Name;
end;

{ "dataDir" from a module's plugin-meta.json (Qt writes one key per line, UTF-8, forward slashes). }
function DataDirOf(Name: String): String;
var
  Lines: TArrayOfString;
  I, P: Integer;
  S: String;
begin
  Result := ConfigDir(Name);
  if not LoadStringsFromFile(ConfigDir(Name) + '\plugin-meta.json', Lines) then
    Exit;
  for I := 0 to GetArrayLength(Lines) - 1 do
  begin
    S := Trim(Lines[I]);
    if Pos('"dataDir": "', S) = 1 then
    begin
      S := Copy(S, Length('"dataDir": "') + 1, MaxInt);
      P := Pos('"', S);
      if P > 1 then
      begin
        S := Copy(S, 1, P - 1);
        StringChangeEx(S, '\\', '\', True);
        StringChangeEx(S, '/', '\', True);
        Result := S;
      end;
      Exit;
    end;
  end;
end;

{ ---------- VST Enigmax Loader filters still in use ---------- }

function CountIn(const Text, Pattern: String): Integer;
var
  S: String;
  P: Integer;
begin
  Result := 0;
  S := Text;
  P := Pos(Pattern, S);
  while P > 0 do
  begin
    Result := Result + 1;
    Delete(S, 1, P + Length(Pattern) - 1);
    P := Pos(Pattern, S);
  end;
end;

function CountLoaderFilters: Integer;
var
  Dir: String;
  Rec: TFindRec;
  Text: AnsiString;
begin
  Result := 0;
  Dir := ExpandConstant('{userappdata}\obs-studio\basic\scenes\');
  if FindFirst(Dir + '*.json', Rec) then
  try
    repeat
      if LoadStringFromFile(Dir + Rec.Name, Text) then
        Result := Result + CountIn(String(Text), '"id": "vst_ex_audio_filter"') +
                  CountIn(String(Text), '"id":"vst_ex_audio_filter"');
    until not FindNext(Rec);
  finally
    FindClose(Rec);
  end;
end;

{ True = go on. }
function ConfirmLoaderRemoval: Boolean;
var
  N: Integer;
begin
  Result := True;
  N := CountLoaderFilters;
  if N > 0 then
    Result := MsgBox(FmtMessage(CustomMessage('LoaderFiltersLost'), [IntToStr(N)]), mbConfirmation, MB_YESNO) = IDNO;
end;

{ ---------- setup ---------- }

function InitializeSetup: Boolean;
begin
  Result := WaitForObsClosed;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectComponents) and not WizardIsComponentSelected('loader') and
     DirExists(ExpandConstant('{app}\{#LoaderName}')) then
    Result := ConfirmLoaderRemoval;
end;

{ ---------- uninstall: what to keep ---------- }

procedure AskWhatToDelete;
var
  Form: TSetupForm;
  Intro, WhereLabel: TNewStaticText;
  Thumbs, Shots, Settings, Exp: TNewCheckBox;
  Where: TNewEdit;
  Ok: TNewButton;
  Y: Integer;
begin
  Form := CreateCustomForm(ScaleX(460), ScaleY(250), False, False);
  try
    Form.Caption := CustomMessage('KeepTitle');
    Intro := TNewStaticText.Create(Form);
    Intro.Parent := Form;
    Intro.Left := ScaleX(12);
    Intro.Top := ScaleY(12);
    Intro.Width := Form.ClientWidth - ScaleX(24);
    Intro.AutoSize := False;
    Intro.WordWrap := True;
    Intro.Height := ScaleY(30);
    Intro.Caption := CustomMessage('KeepIntro');
    Y := ScaleY(48);
    Thumbs := TNewCheckBox.Create(Form);
    Shots := TNewCheckBox.Create(Form);
    Settings := TNewCheckBox.Create(Form);
    Exp := TNewCheckBox.Create(Form);
    Thumbs.Caption := CustomMessage('DelThumbs');
    Shots.Caption := CustomMessage('DelShots');
    Settings.Caption := CustomMessage('DelSettings');
    Exp.Caption := CustomMessage('ExportFirst');
    Thumbs.Parent := Form; Shots.Parent := Form; Settings.Parent := Form; Exp.Parent := Form;
    Thumbs.Left := ScaleX(12); Shots.Left := ScaleX(12); Settings.Left := ScaleX(12); Exp.Left := ScaleX(12);
    Thumbs.Width := Form.ClientWidth - ScaleX(24); Shots.Width := Thumbs.Width;
    Settings.Width := Thumbs.Width; Exp.Width := Thumbs.Width;
    Thumbs.Top := Y; Shots.Top := Y + ScaleY(24); Settings.Top := Y + ScaleY(48); Exp.Top := Y + ScaleY(80);
    WhereLabel := TNewStaticText.Create(Form);
    WhereLabel.Parent := Form;
    WhereLabel.Left := ScaleX(30);
    WhereLabel.Top := Y + ScaleY(104);
    WhereLabel.Caption := CustomMessage('ExportWhere');
    Where := TNewEdit.Create(Form);
    Where.Parent := Form;
    Where.Left := ScaleX(30);
    Where.Top := Y + ScaleY(122);
    Where.Width := Form.ClientWidth - ScaleX(42);
    Where.Text := ExpandConstant('{userdesktop}');
    Ok := TNewButton.Create(Form);
    Ok.Parent := Form;
    Ok.Caption := CustomMessage('OK');
    Ok.ModalResult := mrOk;
    Ok.Width := ScaleX(90);
    Ok.Height := ScaleY(25);
    Ok.Left := Form.ClientWidth - Ok.Width - ScaleX(12);
    Ok.Top := Form.ClientHeight - Ok.Height - ScaleY(10);
    Form.ActiveControl := Ok;
    Form.ShowModal;
    DelThumbs := Thumbs.Checked;
    DelShots := Shots.Checked;
    DelSettings := Settings.Checked;
    ExportFirst := Exp.Checked;
    ExportDir := Trim(Where.Text);
  finally
    Form.Free;
  end;
end;

{ ---------- uninstall: which files are ours ---------- }

function IsHex(const S: String; AllowDash: Boolean): Boolean;
var
  I: Integer;
begin
  Result := Length(S) > 0;
  for I := 1 to Length(S) do
    if not (((S[I] >= '0') and (S[I] <= '9')) or ((S[I] >= 'a') and (S[I] <= 'f')) or (AllowDash and (S[I] = '-'))) then
      Result := False;
end;

{ Our picture names: <40 hex sha1>.png or filter-<uuid>.png, with -full before .png in screenshots\. }
function IsOurPicture(Name: String; Full: Boolean): Boolean;
var
  Base: String;
begin
  Result := False;
  if Lowercase(ExtractFileExt(Name)) <> '.png' then
    Exit;
  Base := ChangeFileExt(Name, '');
  if Full then
  begin
    if Copy(Base, Length(Base) - 4, 5) <> '-full' then
      Exit;
    Base := Copy(Base, 1, Length(Base) - 5);
  end;
  if Copy(Base, 1, 7) = 'filter-' then
    Result := (Length(Base) = 7 + 36) and IsHex(Copy(Base, 8, 36), True)
  else
    Result := (Length(Base) = 40) and IsHex(Base, False);
end;

{ Files the installer puts in a plugin folder, relative to it. Inno removes these itself. }
function IsInstalledFile(const Rel: String): Boolean;
var
  N: String;
begin
  N := Lowercase(Rel);
  Result := (N = Lowercase('bin\64bit\{#MainName}.dll')) or (N = Lowercase('bin\64bit\{#MainName}.pdb')) or
            (N = 'bin\64bit\{#LoaderName}.dll') or (N = 'bin\64bit\{#LoaderName}.pdb') or
            (N = 'bin\64bit\vst-ex-scanner.exe') or (Copy(N, 1, 15) = 'uninstall\unins') or
            ((Copy(N, 1, 12) = 'data\locale\') and
             (Pos(' ' + Copy(N, 13, MaxInt) + ' ', ' en-us.ini fr-fr.ini id-id.ini ja-jp.ini ko-kr.ini zh-tw.ini ') > 0));
end;

{ Subfolders the installer creates inside a plugin folder; any other subfolder is not ours. }
function IsInstalledDir(const Rel: String): Boolean;
var
  N: String;
begin
  N := Lowercase(Rel);
  Result := (N = 'bin') or (N = 'bin\64bit') or (N = 'data') or (N = 'data\locale') or (N = 'uninstall');
end;

{ category: 1 thumbnails, 2 screenshots, 3 settings/history/presets, 4 installed file, 0 not ours }
function CategoryOf(const Kind, Rel: String): Integer;
var
  N: String;
begin
  Result := 0;
  N := Lowercase(Rel);
  if Kind = 'thumbnails' then begin if IsOurPicture(N, False) then Result := 1; end
  else if Kind = 'screenshots' then begin if IsOurPicture(N, True) then Result := 2; end
  else if Kind = 'history' then begin
    if (ExtractFileExt(N) = '.json') and (Length(N) = 36 + 5) and IsHex(Copy(N, 1, 36), True) then Result := 3; end
  else if Kind = 'presets' then begin if N = 'chain-presets.json' then Result := 3; end
  else if Kind = 'config' then begin
    if (N = 'plugin-meta.json') or (N = 'plugin-cache.json') or (N = 'install-features.ini') or
       (N = 'vst-ex.log') or (N = 'vst-ex.log.old') then Result := 3; end
  else if Kind = 'plugin' then begin if IsInstalledFile(N) then Result := 4; end;
end;

{ ---------- uninstall: the folders we look at ---------- }

var
  Roots, Kinds: TStringList;   { folder + what kind of files belong there }
  Strays: TStringList;         { files that are not ours (may be deleted, if the user ticks them) }
  KeptDirs: TStringList;       { folders that are not ours: never opened, never deleted }
  ToDelete: TStringList;       { strays the user ticked, then our files of the ticked categories }

procedure AddRoot(const Dir, Kind: String);
begin
  if (Dir <> '') and DirExists(Dir) and (Roots.IndexOf(Dir + '|' + Kind) < 0) then
  begin
    Roots.Add(Dir + '|' + Kind);
    Kinds.Add(Kind);
  end;
end;

procedure CollectRoots;
var
  MainData, LoaderData: String;
begin
  MainData := DataDirOf('{#MainName}');
  LoaderData := DataDirOf('{#LoaderName}');
  AddRoot(ConfigDir('{#MainName}'), 'config');
  AddRoot(ConfigDir('{#LoaderName}'), 'config');
  AddRoot(LoaderData + '\thumbnails', 'thumbnails');
  AddRoot(LoaderData + '\screenshots', 'screenshots');
  AddRoot(MainData + '\history', 'history');
  AddRoot(MainData + '\presets', 'presets');
  AddRoot(ExpandConstant('{app}\{#MainName}'), 'plugin');
  AddRoot(ExpandConstant('{app}\{#LoaderName}'), 'plugin');
end;

function RootDir(I: Integer): String;
begin
  Result := Copy(Roots[I], 1, Pos('|', Roots[I]) - 1);
end;

{ Lists what is not ours in one root. Only a plugin folder's own subfolders are entered; any other folder
  (a config or data folder can hold the user's own) is noted as kept and its contents are never looked at. }
procedure ScanDir(const Root, Kind, Rel: String);
var
  Rec: TFindRec;
  Path, Sub: String;
begin
  Path := Root;
  if Rel <> '' then
    Path := Root + '\' + Rel;
  if FindFirst(Path + '\*', Rec) then
  try
    repeat
      if (Rec.Name = '.') or (Rec.Name = '..') then
        Continue;
      Sub := Rec.Name;
      if Rel <> '' then
        Sub := Rel + '\' + Rec.Name;
      if (Rec.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
      begin
        if (Kind = 'plugin') and IsInstalledDir(Sub) then
          ScanDir(Root, Kind, Sub)
        else if (Kind <> 'config') or ((Lowercase(Sub) <> 'thumbnails') and (Lowercase(Sub) <> 'screenshots') and
                (Lowercase(Sub) <> 'history') and (Lowercase(Sub) <> 'presets')) then
          KeptDirs.Add(Root + '\' + Sub); { the data folders inside a config folder are roots of their own }
      end
      else if CategoryOf(Kind, Sub) = 0 then
        Strays.Add(Root + '\' + Sub);
    until not FindNext(Rec);
  finally
    FindClose(Rec);
  end;
end;

{ Queues our files of the ticked categories in a root (its top level only; our files are never deeper). }
procedure QueueOurs(const Root, Kind: String);
var
  Rec: TFindRec;
  Cat: Integer;
begin
  if Kind = 'plugin' then
    Exit; { Inno removes the installed files itself }
  if FindFirst(Root + '\*', Rec) then
  try
    repeat
      if (Rec.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
        Continue;
      Cat := CategoryOf(Kind, Rec.Name);
      if ((Cat = 1) and DelThumbs) or ((Cat = 2) and DelShots) or ((Cat = 3) and DelSettings) then
        ToDelete.Add(Root + '\' + Rec.Name);
    until not FindNext(Rec);
  finally
    FindClose(Rec);
  end;
end;

{ ---------- uninstall: asking ---------- }

function SaveStrayList: String;
var
  Lines: TArrayOfString;
  I, N: Integer;
begin
  Result := ExpandConstant('{userdesktop}\VST-Ex-uninstall-leftovers.txt');
  SetArrayLength(Lines, Strays.Count + KeptDirs.Count + 4);
  N := 0;
  Lines[N] := '[files]'; N := N + 1;
  for I := 0 to Strays.Count - 1 do begin Lines[N] := Strays[I]; N := N + 1; end;
  Lines[N] := ''; N := N + 1;
  Lines[N] := '[folders]'; N := N + 1;
  for I := 0 to KeptDirs.Count - 1 do begin Lines[N] := KeptDirs[I]; N := N + 1; end;
  SaveStringsToUTF8File(Result, Lines, False);
end;

procedure SetAll(List: TNewCheckListBox; On: Boolean);
var
  I: Integer;
begin
  for I := 0 to List.Items.Count - 1 do
    List.Checked[I] := On;
end;

var
  StrayList: TNewCheckListBox;

procedure SelectAllClick(Sender: TObject);
begin
  SetAll(StrayList, True);
end;

procedure SelectNoneClick(Sender: TObject);
begin
  SetAll(StrayList, False);
end;

{ Tick the strays to delete; nothing is ticked at first. }
procedure ChooseStrays;
var
  Form: TSetupForm;
  Text: TNewStaticText;
  All, None, Del, Keep: TNewButton;
  I: Integer;
begin
  Form := CreateCustomForm(ScaleX(620), ScaleY(400), True, True);
  try
    Form.Caption := CustomMessage('KeepTitle');
    Text := TNewStaticText.Create(Form);
    Text.Parent := Form;
    Text.Left := ScaleX(12);
    Text.Top := ScaleY(10);
    Text.Width := Form.ClientWidth - ScaleX(24);
    Text.Height := ScaleY(44);
    Text.AutoSize := False;
    Text.WordWrap := True;
    Text.Caption := CustomMessage('Strays');
    StrayList := TNewCheckListBox.Create(Form);
    StrayList.Parent := Form;
    StrayList.Left := ScaleX(12);
    StrayList.Top := ScaleY(58);
    StrayList.Width := Form.ClientWidth - ScaleX(24);
    StrayList.Height := Form.ClientHeight - ScaleY(58) - ScaleY(46);
    for I := 0 to Strays.Count - 1 do
      StrayList.AddCheckBox(Strays[I], '', 0, False, True, False, False, nil);
    All := TNewButton.Create(Form);
    All.Parent := Form;
    All.Caption := CustomMessage('SelectAll');
    All.Width := ScaleX(100);
    All.Height := ScaleY(25);
    All.Left := ScaleX(12);
    All.Top := Form.ClientHeight - All.Height - ScaleY(10);
    All.OnClick := @SelectAllClick;
    None := TNewButton.Create(Form);
    None.Parent := Form;
    None.Caption := CustomMessage('SelectNone');
    None.Width := ScaleX(100);
    None.Height := ScaleY(25);
    None.Left := All.Left + All.Width + ScaleX(8);
    None.Top := All.Top;
    None.OnClick := @SelectNoneClick;
    Keep := TNewButton.Create(Form);
    Keep.Parent := Form;
    Keep.Caption := CustomMessage('KeepAll');
    Keep.ModalResult := mrCancel;
    Keep.Width := ScaleX(100);
    Keep.Height := ScaleY(25);
    Keep.Top := All.Top;
    Keep.Left := Form.ClientWidth - ScaleX(12) - Keep.Width;
    Del := TNewButton.Create(Form);
    Del.Parent := Form;
    Del.Caption := CustomMessage('DeleteTicked');
    Del.ModalResult := mrOk;
    Del.Width := ScaleX(160);
    Del.Height := ScaleY(25);
    Del.Top := All.Top;
    Del.Left := Keep.Left - ScaleX(8) - Del.Width;
    Form.ActiveControl := Keep;
    if Form.ShowModal = mrOk then
      for I := 0 to Strays.Count - 1 do
        if StrayList.Checked[I] then
          ToDelete.Add(Strays[I]);
  finally
    Form.Free;
  end;
end;

{ False = stop the uninstall (the list was saved for the user to look at). }
function AskAboutStrays: Boolean;
var
  Labels: TArrayOfString;
  Choice: Integer;
begin
  Result := True;
  if (Strays.Count = 0) and (KeptDirs.Count = 0) then
    Exit;
  SetArrayLength(Labels, 3);
  Labels[0] := CustomMessage('StraysKeep');
  Labels[1] := CustomMessage('StraysChoose');
  Labels[2] := CustomMessage('StraysSave');
  Choice := TaskDialogMsgBox(CustomMessage('KeepTitle'),
    FmtMessage(CustomMessage('StraysFound'), [IntToStr(Strays.Count), IntToStr(KeptDirs.Count)]),
    mbConfirmation, MB_YESNOCANCEL, Labels, 0);
  if Choice = IDNO then
  begin
    if Strays.Count > 0 then
      ChooseStrays;
  end
  else if Choice = IDCANCEL then
  begin
    MsgBox(FmtMessage(CustomMessage('StraysSaved'), [SaveStrayList]), mbInformation, MB_OK);
    Result := False;
  end;
end;

{ ---------- uninstall: copying and removing ---------- }

procedure CopyFiles(const FromDir, ToDir: String);
var
  Rec: TFindRec;
begin
  if FindFirst(FromDir + '\*', Rec) then
  try
    repeat
      if (Rec.Attributes and FILE_ATTRIBUTE_DIRECTORY) = 0 then
      begin
        ForceDirectories(ToDir);
        FileCopy(FromDir + '\' + Rec.Name, ToDir + '\' + Rec.Name, False);
      end;
    until not FindNext(Rec);
  finally
    FindClose(Rec);
  end;
end;

{ Same layout as Tools > #cbOBScripts > VST Ex Audio Filter > Debug mode > Backup. Only top-level files: ours are never deeper,
  and folders that are not ours (possibly huge) are not dragged along. }
procedure ExportAll;
var
  Dest: String;
  Ini: TArrayOfString;
begin
  Dest := ExportDir + '\VST-Ex-backup-' + GetDateTimeString('yyyymmdd-hhnnss', #0, #0);
  CopyFiles(ConfigDir('{#MainName}'), Dest + '\config\{#MainName}');
  CopyFiles(ConfigDir('{#LoaderName}'), Dest + '\config\{#LoaderName}');
  CopyFiles(DataDirOf('{#LoaderName}') + '\thumbnails', Dest + '\data\thumbnails');
  CopyFiles(DataDirOf('{#LoaderName}') + '\screenshots', Dest + '\data\screenshots');
  CopyFiles(DataDirOf('{#MainName}') + '\history', Dest + '\data\history');
  CopyFiles(DataDirOf('{#MainName}') + '\presets', Dest + '\data\presets');
  SetArrayLength(Ini, 2);
  Ini[0] := '[backup]';
  Ini[1] := 'version=1';
  SaveStringsToUTF8FileWithoutBOM(Dest + '\vst-ex-backup.ini', Ini, False);
  MsgBox(FmtMessage(CustomMessage('Exported'), [Dest]), mbInformation, MB_OK);
end;

{ Removes the folders we made, deepest first, and only when empty: anything left inside keeps its folder. }
procedure RemoveOurEmptyDirs;
var
  I: Integer;
  Dir: String;
begin
  for I := Roots.Count - 1 downto 0 do
  begin
    Dir := RootDir(I);
    if Kinds[I] = 'plugin' then
    begin
      RemoveDir(Dir + '\bin\64bit');
      RemoveDir(Dir + '\bin');
      RemoveDir(Dir + '\data\locale');
      RemoveDir(Dir + '\data');
      RemoveDir(Dir + '\uninstall');
    end;
    RemoveDir(Dir);
  end;
end;

{ ---------- uninstall: flow ---------- }

function InitializeUninstall: Boolean;
var
  I: Integer;
begin
  Result := WaitForObsClosed and ConfirmLoaderRemoval;
  if not Result then
    Exit;
  Roots := TStringList.Create;
  Kinds := TStringList.Create;
  Strays := TStringList.Create;
  KeptDirs := TStringList.Create;
  ToDelete := TStringList.Create;
  CollectRoots;
  if UninstallSilent then
    Exit; { silent: keep everything that is not ours, delete nothing extra }
  { Looked at before anything is removed, so the uninstall can still stop cleanly here. }
  for I := 0 to Roots.Count - 1 do
    ScanDir(RootDir(I), Kinds[I], '');
  Result := AskAboutStrays;
  if Result then
    AskWhatToDelete;
end;

{ Sends the files to the Recycle Bin, so a wrong tick can be undone. If PowerShell fails, the files stay where they are. }
procedure RecycleFiles(Files: TStringList);
var
  Lines: TArrayOfString;
  ListFile, Cmd: String;
  I, Code: Integer;
begin
  if Files.Count = 0 then
    Exit;
  SetArrayLength(Lines, Files.Count);
  for I := 0 to Files.Count - 1 do
    Lines[I] := Files[I];
  ListFile := ExpandConstant('{tmp}\recycle-list.txt');
  if not SaveStringsToUTF8File(ListFile, Lines, False) then
    Exit;
  StringChangeEx(ListFile, '''', '''''', True);
  Cmd := '-NoProfile -ExecutionPolicy Bypass -Command "Add-Type -AssemblyName Microsoft.VisualBasic; ' +
    'Get-Content -LiteralPath ''' + ListFile + ''' -Encoding UTF8 | ForEach-Object { ' +
    'if (Test-Path -LiteralPath $_ -PathType Leaf) { [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteFile($_, ''OnlyErrorDialogs'', ''SendToRecycleBin'') } }"';
  if not Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'), Cmd, '', SW_HIDE, ewWaitUntilTerminated, Code) or (Code <> 0) then
    Log('Recycle Bin move failed (code ' + IntToStr(Code) + '); the files were left in place');
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  I: Integer;
begin
  if CurUninstallStep <> usPostUninstall then
    Exit;
  if ExportFirst and (DelThumbs or DelShots or DelSettings) then
    ExportAll;
  for I := 0 to Roots.Count - 1 do
    QueueOurs(RootDir(I), Kinds[I]);
  RecycleFiles(ToDelete);
  RemoveOurEmptyDirs;
end;
