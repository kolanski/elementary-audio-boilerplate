// import {Renderer, el} from '@elemaudio/core';
let counter = 0;
function logRandomNumber() {
    setInterval("fact", 1000);
  }
  
  logRandomNumber();
function fact()
{
    counter++;
    console.log("fact"+counter);
}
globalThis.fact = fact
console.log("test")
// // Создаем рендерер
// let core = new Renderer((batch) => {
//   __postNativeMessage__(JSON.stringify(batch));
// });

// // Создаем ref для управления громкостью
// let [volumeRef, setVolumeProps] = core.createRef("const", { value: 0.01 }, []);

// // Функция для задержки (эмуляция setTimeout)
// function delay(ms) {
//   return new Promise(resolve => setTimeout(resolve, ms));
// }

// // Асинхронная функция для изменения громкости
// async function updateVolume() {
//   while (true) {
//     // Ограничиваем значение громкости в пределах от 0 до 1
//     const newValue = Math.min(1.0, volumeRef.value + 0.2);
//     setVolumeProps({ value: newValue });

//     // Обновляем аудиограф с новым значением громкости
//     renderAudio();

//     // Задержка перед следующим обновлением
//     await delay(3000);
//   }
// }

// function renderAudio() {
//   core.render(
//     el.mul(volumeRef, el.cycle(440))
//   );
// }

// // Первоначальный рендер
// renderAudio();

// // Запуск асинхронного процесса обновления громкости
// updateVolume();