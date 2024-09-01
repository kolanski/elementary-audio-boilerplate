import {Renderer, el} from '@elemaudio/core';
let counter = 0;
let base = 0.01;
let volumeRef;
let setVolumeProps;
function logRandomNumber() {
    //setInterval("fact", 1000);
  }
  
  logRandomNumber();
function fact()
{
    counter++;
    console.log("fact"+counter);
    //updateVolume();
}
function update_volume(new_value)
{

  const clampedValue = Math.max(0, Math.min(1, new_value));
  setVolumeProps({ value: clampedValue });
}
globalThis.fact = fact;
globalThis.update_volume = update_volume;
globalThis.updateVolume = updateVolume;
console.log("test")
// Создаем рендерер
let core = new Renderer((batch) => {
  __postNativeMessage__(JSON.stringify(batch));
});

// Создаем ref для управления громкостью
[volumeRef, setVolumeProps] = core.createRef("const", { value: 0.0 }, []);

 // Функция для задержки (эмуляция setTimeout)
 // Асинхронная функция для изменения громкости
 async function updateVolume() {
     // Ограничиваем значение громкости в пределах от 0 до 1
     let newValue = base;
     base += 0.05;
     setVolumeProps({ value: newValue });
     // Обновляем аудиограф с новым значением громкости
     //renderAudio();
     // Задержка перед следующим обновлением
 }
 function renderAudio() {
   core.render(
     el.mul(volumeRef, el.cycle(440))
   );
 }
 // Первоначальный рендер
 renderAudio();
 // Запуск асинхронного процесса обновления громкости