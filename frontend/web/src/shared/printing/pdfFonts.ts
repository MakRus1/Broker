import { interfaceText } from '../localization/interfaceText';
import regularUrl from './fonts/NotoSans-Regular.ttf?url';
import boldUrl from './fonts/NotoSans-Bold.ttf?url';
async function font(url: string): Promise<string> {
  const response = await fetch(url);
  if (!response.ok) throw new Error(interfaceText('print.fontFailed'));
  const bytes = new Uint8Array(await response.arrayBuffer());
  let binary = '';
  for (let offset = 0; offset < bytes.length; offset += 8192)
    binary += String.fromCharCode(...bytes.subarray(offset, offset + 8192));
  return btoa(binary);
}
let fonts: Promise<{ regular: string; bold: string }> | undefined;
export async function loadPdfFonts() {
  fonts ??= Promise.all([font(regularUrl), font(boldUrl)])
    .then(([regular, bold]) => ({ regular, bold }))
    .catch((error) => {
      fonts = undefined;
      throw error;
    });
  return fonts;
}
